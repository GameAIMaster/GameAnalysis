using UnityEngine;

namespace NaughtyWaterBuoyancy
{
    public static class MathfUtils
    {
        public static float GetAverageValue(params float[] values)
        {
            float sum = 0;
            for (int i = 0; i < values.Length; i++)
            {
                sum += values[i];
            }

            return sum / values.Length;
        }

        public static Vector3 GetAveratePoint(params Vector3[] points)
        {
            Vector3 sum = Vector3.zero;
            for (int i = 0; i < points.Length; i++)
            {
                sum += points[i];
            }

            return sum / points.Length;
        }

        public static bool IsPointInsideTriangle(Vector3 point, Vector3 tp1, Vector3 tp2, Vector3 tp3)
        {
            float trueArea = CalculateArea_Triangle(tp1, tp2, tp3);

            float checkArea =
                CalculateArea_Triangle(point, tp1, tp2) +
                CalculateArea_Triangle(point, tp2, tp3) +
                CalculateArea_Triangle(point, tp3, tp1);

            return Mathf.Abs(trueArea - checkArea) < 0.01f;
        }

        public static bool IsPointInsideTriangle(Vector3 point, Vector3 tp1, Vector3 tp2, Vector3 tp3, bool ignoreX, bool ignoreY, bool ignoreZ)
        {
            if (ignoreX)
            {
                point.x = 0f;
                tp1.x = 0f;
                tp2.x = 0f;
                tp3.x = 0f;
            }

            if (ignoreY)
            {
                point.y = 0f;
                tp1.y = 0f;
                tp2.y = 0f;
                tp3.y = 0f;
            }

            if (ignoreZ)
            {
                point.z = 0f;
                tp1.z = 0f;
                tp2.z = 0f;
                tp3.z = 0f;
            }

            return IsPointInsideTriangle(point, tp1, tp2, tp3);
        }

        public static bool IsPointInsideTriangle(Vector3 point, Vector3[] triangle)
        {
            return IsPointInsideTriangle(point, triangle[0], triangle[1], triangle[2]);
        }

        public static bool IsPointInsideTriangle(Vector3 point, Vector3[] triangle, bool ignoreX, bool ignoreY, bool ignoreZ)
        {
            return IsPointInsideTriangle(point, triangle[0], triangle[1], triangle[2], ignoreX, ignoreY, ignoreZ);
        }

        public static float CalculateArea_Triangle(Vector3 p1, Vector3 p2, Vector3 p3)
        {
            float a = (p1 - p2).magnitude;
            float b = (p1 - p3).magnitude;
            float c = (p2 - p3).magnitude;
            float p = (a + b + c) / 2f; // The half perimeter

            return Mathf.Sqrt(p * (p - a) * (p - b) * (p - c));
        }

        public static float CalculateArea_Triangle(Vector3[] triangle)
        {
            return CalculateArea_Triangle(triangle[0], triangle[1], triangle[2]);
        }

        public static float CalculateVolume_Mesh(Mesh mesh, Transform trans)
        {
            float volume = 0f;
            Vector3[] vertices = mesh.vertices;
            int[] triangles = mesh.triangles;
            for (int i = 0; i < mesh.triangles.Length; i += 3)
            {
                Vector3 p1 = vertices[triangles[i + 0]];
                Vector3 p2 = vertices[triangles[i + 1]];
                Vector3 p3 = vertices[triangles[i + 2]];

                volume += CalculateVolume_Tetrahedron(p1, p2, p3, Vector3.zero);
            }

            return Mathf.Abs(volume) * trans.localScale.x * trans.localScale.y * trans.localScale.z;
        }

		public static Vector3 CalculateVolume_MeshCenter(Mesh mesh)
		{
			//求mesh的几何中心
			float volume = 0f;
			Vector3 MeshCenter = new Vector3(0,0,0);
			Vector3[] vertices = mesh.vertices;
			int[] triangles = mesh.triangles;
			float tempVolume = 0f;
			Vector3 tempCenter;
			for (int i = 0; i < mesh.triangles.Length; i += 3)
			{
				Vector3 p1 = vertices[triangles[i + 0]];
				Vector3 p2 = vertices[triangles[i + 1]];
				Vector3 p3 = vertices[triangles[i + 2]];
				tempCenter = CalculateVolume_TetrahedronCenter(p1, p2, p3, Vector3.zero);
				tempVolume = CalculateVolume_Tetrahedron(p1, p2, p3, Vector3.zero);
				MeshCenter += tempCenter * tempVolume;
				volume += tempVolume;
			}
			MeshCenter = MeshCenter / volume;
			return MeshCenter;
		}

		//计算点与一条直线的关系
		//计算三角面与一条直线的关系
		public static float CalculateVolume_UnderWater(Mesh mesh, Vector3 point, Vector3 normal)
		{
			float volume = 0f;
			Vector3[] vertices = mesh.vertices;
			int[] triangles = mesh.triangles;
			int belowNum = 0;
			for (int i = 0; i < mesh.triangles.Length; i += 3)
			{
				Vector3 p1 = vertices[triangles[i + 0]];
				belowNum += Vector3.Dot(p1 - point, normal) <= 0f ? 1 : 0;
				Vector3 p2 = vertices[triangles[i + 1]];
				belowNum += Vector3.Dot(p2 - point, normal) <= 0f ? 1 : 0;
				Vector3 p3 = vertices[triangles[i + 2]];
				belowNum += Vector3.Dot(p3 - point, normal) <= 0f ? 1 : 0;

				//如果全在切面以下
				if (belowNum == 3 )
				{
					volume += CalculateVolume_Tetrahedron(p1, p2, p3, Vector3.zero);
				}
				else if(belowNum == 0)
				{
					//全在平面以下
				}
				else if (belowNum == 1)
				{
					//两个在平面以下
				}
				else
				{
					//一个在平面以下
				}
			}
			return volume;
		}
		public static float CalculateVolume_Tetrahedron(Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4)
        {
			//计算四面体体积
            Vector3 a = p1 - p2;
            Vector3 b = p1 - p3;
            Vector3 c = p1 - p4;

            return (Vector3.Dot(a, Vector3.Cross(b, c))) / 6f;

            ////float v321 = p3.x * p2.y * p1.z;
            ////float v231 = p2.x * p3.y * p1.z;
            ////float v312 = p3.x * p1.y * p2.z;
            ////float v132 = p1.x * p3.y * p2.z;
            ////float v213 = p2.x * p1.y * p3.z;
            ////float v123 = p1.x * p2.y * p3.z;

            ////return (1f / 6f) * (-v321 + v231 + v312 - v132 - v213 + v123);
        }

        public static float CalculateVolume_Tetrahedron(Vector3[] tetrahedron)
        {
            return CalculateVolume_Tetrahedron(tetrahedron[0], tetrahedron[1], tetrahedron[2], tetrahedron[3]);
        }

		public static Vector3 CalculateVolume_TetrahedronCenter(Vector3[] tetrahedron)
		{
			//求四面体几何中心
			return (tetrahedron[0]+ tetrahedron[1]+ tetrahedron[2]+ tetrahedron[3]) / 4; 
		}
		public static Vector3 CalculateVolume_TetrahedronCenter(Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4)
		{
			return (p1 + p2 + p3 + p4) / 4;
		}
	}
}
