#pragma once
#include "stdafx.h"
#include <windows.h>
#include <minwindef.h>
#include <stdio.h>
#include <processthreadsapi.h>
#include <memoryapi.h>
#include <winbase.h>
#include <TlHelp32.h>
#include <winnt.h>
#include "DbgHelp.h"
#include <stdint.h>
//jmpռ�õ��ֽ�
//����memory����������ַ���\0x50����˸�0 Ӧ��Ϊ\x50
//����lib��Ҫʹ���ĸ��������ڻ���������Link�³���������Ŀ¼����Include�м���lib
//ȷ�Ϻ�ʹ�õĻ���
//�ر�RTC�������ɵ��Դ���
//Ctrl+Alt+D�򿪻����롣
#define JMP_CMD_SIZE 6  

char* InsertOpCode(
	char* dst_buffer, char* op_code, uint32_t op_size,
	char* arg, uint32_t arg_size
)
{
	if (op_code)
	{
		memcpy(dst_buffer, op_code, op_size);
	}
	if (arg)
	{
		memcpy(dst_buffer + op_size, arg, arg_size);
	}
	return dst_buffer + op_size + arg_size;
}
//��������

uint32_t GetMoveCodeSize(const char arr[])
{
	return 0;
}

BOOL CheckEIPAndSuspendByTargetProcessId(DWORD process_id, DWORD addr, size_t size);
void RusumeByTargetProcessId(DWORD process_id);

int main()
{
	DWORD process_id;

	scanf_s("%d", &process_id);
	HANDLE process_handle = OpenProcess(
		PROCESS_ALL_ACCESS, TRUE, process_id);

	if (process_handle == 0)
	{
		return -1;
	}

	char code_buffer[1024];
	char* code_offset = NULL;
	SIZE_T ReadBytes = 0;

	BOOL ret_init = SymInitialize(process_handle, NULL, FALSE);
	if (ret_init == FALSE)
	{
		CloseHandle(process_handle);
		return -1;
	}

	BOOL ret_refresh = SymRefreshModuleList(process_handle);
	if (ret_refresh == FALSE)
	{
		SymCleanup(process_handle);
		CloseHandle(process_handle);
		return -1;
	}

	char memory[1024];
	SYMBOL_INFO* symbol_info_point = (SYMBOL_INFO*)memory;
	symbol_info_point->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol_info_point->MaxNameLen = sizeof(memory) - sizeof(SYMBOL_INFO);
	// TODO ��Ӧ�÷���BOOLֵ
	BOOL ret_from = SymFromName(process_handle, "TestProj!TargetFunc", symbol_info_point);
	if (ret_from == FALSE)
	{
		SymCleanup(process_handle);
		CloseHandle(process_handle);
		return 0;
	}
	printf("TestProj!TargetFunc %I64X", symbol_info_point->Address);
	LPVOID addr = (LPVOID)symbol_info_point->Address;
	//
	char move_code[64];
	SIZE_T write_number = 0;
	ReadProcessMemory(process_handle, addr, code_buffer, 64, &ReadBytes);

	uint32_t move_size = 6; // ͨ�����鿴������һ�����ռ���ֽ���   movesize����С��6����Ϊ��Ҫ��ת       GetMoveCodeSize(code_buffer);
	LPVOID restore_addr = (LPVOID)((char*)addr + move_size);
	memcpy(move_code, code_buffer, move_size);

	// ��ʱʹ�õ��ڴ��
	LPVOID Tick = VirtualAllocEx(
		process_handle, NULL, sizeof(uint64_t),
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);

	LPVOID TickHigh32 = (LPVOID)((int)Tick + 4);

	// ������ʹ�õ��ڴ��
	LPVOID Ret = VirtualAllocEx(
		process_handle, NULL, sizeof(uint64_t),
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);

	LPVOID RetHigh32 = (LPVOID)((int)Ret + 4);

	//��ת
	LPVOID restore_jmp = VirtualAllocEx(
		process_handle, NULL, sizeof(ptrdiff_t),
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);
	LPVOID code_jmp = VirtualAllocEx(
		process_handle, NULL, sizeof(ptrdiff_t),
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);


	// ����ʱͳ�Ƶ�ע����룬���õ�ԭ���Ǻ��������л������һ�������ĵ�ַ����һ������ִ�����ָ�����һ����������ǰ��״̬
	code_offset = code_buffer;
	//push eax; push edx; push ecx; 
	code_offset = InsertOpCode(code_offset, "\x50\x51\x52", 3, NULL, 0);
	//rdtscp
	code_offset = InsertOpCode(code_offset, "\x0F\x01\xF9", 3, NULL, 0);
	//sub eax, [Tick] ����
	code_offset = InsertOpCode(code_offset, "\x2B\x05", 2, (char*)&Tick, 4);
	//sbb edx,[Tick + 4] sbb�Ǵ���λ����ָ��
	code_offset = InsertOpCode(code_offset, "\x1B\x15", 2, (char*)&TickHigh32, 4);
	//mov [Tick], eax
	code_offset = InsertOpCode(code_offset, "\x89\x05", 2, (char*)&Ret, 4);
	//mov [Tick + 4], edx 
	code_offset = InsertOpCode(code_offset, "\x89\x15", 2, (char*)&RetHigh32, 4);
	//pop ecx; push edx; push eax
	code_offset = InsertOpCode(code_offset, "\xC3", 1, NULL, 0);
	uint32_t ret_call_size = code_offset - code_buffer;

	LPVOID ret_call = VirtualAllocEx(
		process_handle, NULL, ret_call_size,
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);

	WriteProcessMemory(
		process_handle, ret_call,
		code_buffer, ret_call_size, &write_number);

	//��ʼͳ�Ƶ�ע�����
	code_offset = code_buffer;
	//add esp, 4
	code_offset = InsertOpCode(code_offset, "\x68", 1, (char*)&ret_call, 4);
	//push eax; push edx; push ecx;
	code_offset = InsertOpCode(code_offset, "\x50\x51\x52", 3, NULL, 0);
	//rdtscp
	code_offset = InsertOpCode(code_offset, "\x0F\x01\xF9", 3, NULL, 0);
	//mov [Tick], eax
	code_offset = InsertOpCode(code_offset, "\x89\x05", 2, (char*)&Tick, 4);
	//mov [Tick + 4], edx 
	code_offset = InsertOpCode(code_offset, "\x89\x15", 2, (char*)&TickHigh32, 4);
	//pop ecx; push edx; push eax
	code_offset = InsertOpCode(code_offset, "\x5A\x59\x58", 3, NULL, 0);
	code_offset = InsertOpCode(code_offset, move_code, move_size, NULL, 0);
	code_offset = InsertOpCode(code_offset, "\xFF\x25", 2, (char*)&restore_jmp, 4);
	uint32_t code_size = code_offset - code_buffer;

	LPVOID code = VirtualAllocEx(
		process_handle, NULL, code_size,
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);

	WriteProcessMemory(
		process_handle, code,
		code_buffer, code_size, &write_number);

	//��������ת��ĵ�ַ�����ڴ��У� ����jmpʱʹ���ڴ��ַ
	//LPVOID test = (LPVOID)_byteswap_ulong((unsigned long)restore_addr);
	//LPVOID test1 = (LPVOID)_byteswap_ulong((unsigned long)code);
	WriteProcessMemory(
		process_handle, restore_jmp,
		&restore_addr, sizeof(ptrdiff_t), &write_number);
	WriteProcessMemory(
		process_handle, code_jmp,
		&code, sizeof(ptrdiff_t), &write_number);

	// Hookԭ����ַ�����ݣ���ת����ʼͳ�ƵĴ��봦
	code_offset = code_buffer;
	code_offset = code_offset = InsertOpCode(code_offset, "\xFF\x25", 2, (char*)&code_jmp, 4);
	for (int i = JMP_CMD_SIZE; i < move_size; i++)
	{
		InsertOpCode(code_offset, "\x90", 1, NULL, 0);
	}

	//��ͣԶ�̽��̣���ȷ����ǰ��Eipû��ָ��ע���Ŀ���ַ
	//���ʧ�ܣ� ��ָ�Ŀ����̣�yield��ǰ����
	while (!CheckEIPAndSuspendByTargetProcessId(process_id, (DWORD)addr, move_size))
	{
		RusumeByTargetProcessId(process_id);
		Sleep(1);// ���Ǽ򵥵�ʹ��ϵͳ��Sleep������yield��Ч��

	}

	WriteProcessMemory(
		process_handle, addr,
		code_buffer, move_size, &write_number);

	RusumeByTargetProcessId(process_id);

	uint64_t tick_num = 0;
	ReadProcessMemory(process_handle, Ret, &tick_num, sizeof(uint64_t), &ReadBytes);
	printf("execute Tick = %I64d", tick_num);

	SymCleanup(process_handle);
	CloseHandle(process_handle);
	system("pause");
	return 0;
}

BOOL CheckEIPAndSuspendByTargetProcessId(DWORD process_id, DWORD addr, size_t size)
{
	BOOL ret = FALSE;
	//����ͨ����ȡ������ϢΪָ���Ľ��̡�����ʹ�õĶ�[HEAP]��ģ��[MODULE]���߳̽���һ�����ա�
	HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	THREADENTRY32 th32;
	th32.dwSize = sizeof(THREADENTRY32);
	bool bOK = false;
	//�ҵ����ӽ����е������߳�
	for (bOK = Thread32First(hThreadSnap, &th32);bOK; bOK = Thread32Next(hThreadSnap, &th32))
	{
		if (th32.th32OwnerProcessID == process_id)
		{
			CONTEXT context = { 0 };

			HANDLE thread_handle = OpenThread(
				THREAD_ALL_ACCESS, FALSE, th32.th32ThreadID);
			//��ͣ�߳�
			SuspendThread(thread_handle);
			context.ContextFlags = CONTEXT_CONTROL;
			ret = GetThreadContext(thread_handle, &context);
			if (!ret)
			{
				continue;
			}

			if (context.Eip >= addr && context.Eip <= addr + size)
			{
				return  false;
			}
		}
	}

	return true;
}

void RusumeByTargetProcessId(DWORD process_id)
{
	//�ظ��߳�
	HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
	{
		return ;
	}

	THREADENTRY32 th32;
	th32.dwSize = sizeof(THREADENTRY32);
	bool bOK = false;
	//�ҵ����ӽ����е������߳�
	for (bOK = Thread32First(hThreadSnap, &th32); bOK; bOK = Thread32Next(hThreadSnap, &th32))
	{
		if (th32.th32OwnerProcessID == process_id)
		{
			CONTEXT context = { 0 };

			HANDLE thread_handle = OpenThread(
				THREAD_ALL_ACCESS, FALSE, th32.th32ThreadID);
			ResumeThread(thread_handle);
		}
	}
}
