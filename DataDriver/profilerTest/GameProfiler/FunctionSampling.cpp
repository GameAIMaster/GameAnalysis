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
//jmp占用的字节
//利用memory查出了输入字符串\0x50多加了个0 应该为\x50
//导入lib需要使用哪个环境就在环境下配置Link下常规中引入目录，在Include中加入lib
//确认好使用的环境
//关闭RTC避免生成调试代码
//Ctrl+Alt+D打开汇编代码。
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
//函数采样

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
	// TODO 不应该返回BOOL值
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

	uint32_t move_size = 6; // 通过汇编查看函数第一条汇编占的字节数   movesize不能小于6，因为需要跳转       GetMoveCodeSize(code_buffer);
	LPVOID restore_addr = (LPVOID)((char*)addr + move_size);
	memcpy(move_code, code_buffer, move_size);

	// 计时使用的内存段
	LPVOID Tick = VirtualAllocEx(
		process_handle, NULL, sizeof(uint64_t),
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);

	LPVOID TickHigh32 = (LPVOID)((int)Tick + 4);

	// 结果输出使用的内存段
	LPVOID Ret = VirtualAllocEx(
		process_handle, NULL, sizeof(uint64_t),
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);

	LPVOID RetHigh32 = (LPVOID)((int)Ret + 4);

	//跳转
	LPVOID restore_jmp = VirtualAllocEx(
		process_handle, NULL, sizeof(ptrdiff_t),
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);
	LPVOID code_jmp = VirtualAllocEx(
		process_handle, NULL, sizeof(ptrdiff_t),
		MEM_COMMIT, PAGE_EXECUTE_READWRITE
	);


	// 结束时统计的注入代码，利用的原理是函数调用中会包含下一个函数的地址，下一个函数执行完会恢复到下一个函数调用前的状态
	code_offset = code_buffer;
	//push eax; push edx; push ecx; 
	code_offset = InsertOpCode(code_offset, "\x50\x51\x52", 3, NULL, 0);
	//rdtscp
	code_offset = InsertOpCode(code_offset, "\x0F\x01\xF9", 3, NULL, 0);
	//sub eax, [Tick] 减法
	code_offset = InsertOpCode(code_offset, "\x2B\x05", 2, (char*)&Tick, 4);
	//sbb edx,[Tick + 4] sbb是带借位减法指令
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

	//开始统计的注入代码
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

	//把两个跳转点的地址放入内存中， 方便jmp时使用内存地址
	//LPVOID test = (LPVOID)_byteswap_ulong((unsigned long)restore_addr);
	//LPVOID test1 = (LPVOID)_byteswap_ulong((unsigned long)code);
	WriteProcessMemory(
		process_handle, restore_jmp,
		&restore_addr, sizeof(ptrdiff_t), &write_number);
	WriteProcessMemory(
		process_handle, code_jmp,
		&code, sizeof(ptrdiff_t), &write_number);

	// Hook原来地址的内容，跳转到开始统计的代码处
	code_offset = code_buffer;
	code_offset = code_offset = InsertOpCode(code_offset, "\xFF\x25", 2, (char*)&code_jmp, 4);
	for (int i = JMP_CMD_SIZE; i < move_size; i++)
	{
		InsertOpCode(code_offset, "\x90", 1, NULL, 0);
	}

	//暂停远程进程，并确保当前的Eip没有指向注入的目标地址
	//如果失败， 则恢复目标进程，yield当前进程
	while (!CheckEIPAndSuspendByTargetProcessId(process_id, (DWORD)addr, move_size))
	{
		RusumeByTargetProcessId(process_id);
		Sleep(1);// 我们简单地使用系统的Sleep来带到yield的效果

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
	//可以通过获取进程信息为指定的进程、进程使用的堆[HEAP]、模块[MODULE]、线程建立一个快照。
	HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	THREADENTRY32 th32;
	th32.dwSize = sizeof(THREADENTRY32);
	bool bOK = false;
	//找到监视进程中的所有线程
	for (bOK = Thread32First(hThreadSnap, &th32);bOK; bOK = Thread32Next(hThreadSnap, &th32))
	{
		if (th32.th32OwnerProcessID == process_id)
		{
			CONTEXT context = { 0 };

			HANDLE thread_handle = OpenThread(
				THREAD_ALL_ACCESS, FALSE, th32.th32ThreadID);
			//暂停线程
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
	//回复线程
	HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
	{
		return ;
	}

	THREADENTRY32 th32;
	th32.dwSize = sizeof(THREADENTRY32);
	bool bOK = false;
	//找到监视进程中的所有线程
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
