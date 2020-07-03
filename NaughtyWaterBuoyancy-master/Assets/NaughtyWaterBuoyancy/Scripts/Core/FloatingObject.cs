﻿using System.Collections.Generic;
using UnityEngine;

namespace NaughtyWaterBuoyancy
{
    [RequireComponent(typeof(Collider))]
    [RequireComponent(typeof(Rigidbody))]
    [RequireComponent(typeof(MeshFilter))]
    public class FloatingObject : MonoBehaviour
    {
        [SerializeField]
        private bool calculateDensity = false;

        [SerializeField]
        private float density = 0.75f;

		//浮力力矩参数
		[SerializeField]
		private float ParamTb = 1.0f;

		[SerializeField]
        [Range(0f, 1f)]
        private float normalizedVoxelSize = 0.5f; //单位体积像素大小

        [SerializeField]
        private float dragInWater = 1f;

        [SerializeField]
        private float angularDragInWater = 1f;

        private WaterVolume water;
        private new Collider collider;
        private new Rigidbody rigidbody;
        private float initialDrag;
        private float initialAngularDrag;
        private Vector3 voxelSize;
        private Vector3[] voxels;

        protected virtual void Awake()
        {
            this.collider = this.GetComponent<Collider>();
            this.rigidbody = this.GetComponent<Rigidbody>();

            this.initialDrag = this.rigidbody.drag;
            this.initialAngularDrag = this.rigidbody.angularDrag;

            if (this.calculateDensity)
            {
				//通过碰撞体计算 通过mesh计算
				//MeshCollider collider = this.GetComponent<MeshCollider>();
				//if (collider == null || collider.sharedMesh == null)
				//	return;
				//Mesh mesh0 = collider.sharedMesh;
				//float collisonVolume = MathfUtils.CalculateVolume_Mesh(mesh0, this.transform);

				float objectVolume = MathfUtils.CalculateVolume_Mesh(this.GetComponent<MeshFilter>().mesh, this.transform);
                this.density = this.rigidbody.mass / objectVolume;
            }
        }

        protected virtual void FixedUpdate()
        {
            if (this.water != null && this.voxels.Length > 0)
            {
                Vector3 forceAtSingleVoxel = this.CalculateMaxBuoyancyForce() / this.voxels.Length; //计算浮力
                Bounds bounds = this.collider.bounds;
                float voxelHeight = bounds.size.y * this.normalizedVoxelSize;

                float submergedVolume = 0f;
                for (int i = 0; i < this.voxels.Length; i++)
                {
                    Vector3 worldPoint = this.transform.TransformPoint(this.voxels[i]);
                    
                    float waterLevel = this.water.GetWaterLevel(worldPoint);
                    float deepLevel = waterLevel - worldPoint.y + (voxelHeight / 2f); // How deep is the voxel                    
                    float submergedFactor = Mathf.Clamp(deepLevel / voxelHeight, 0f, 1f); // 0 - voxel is fully out of the water, 1 - voxel is fully submerged
                    submergedVolume += submergedFactor;

                    Vector3 surfaceNormal = this.water.GetSurfaceNormal(worldPoint);
                    Quaternion surfaceRotation = Quaternion.FromToRotation(this.water.transform.up, surfaceNormal);
                    surfaceRotation = Quaternion.Slerp(surfaceRotation, Quaternion.identity, submergedFactor);

                    Vector3 finalVoxelForce = surfaceRotation * (forceAtSingleVoxel * submergedFactor);
                    this.rigidbody.AddForceAtPosition(finalVoxelForce, worldPoint);

                    Debug.DrawLine(worldPoint, worldPoint + finalVoxelForce.normalized, Color.blue);
                }

                submergedVolume /= this.voxels.Length; // 0 - object is fully out of the water, 1 - object is fully submerged

                this.rigidbody.drag = Mathf.Lerp(this.initialDrag, this.dragInWater, submergedVolume);
                this.rigidbody.angularDrag = Mathf.Lerp(this.initialAngularDrag, this.angularDragInWater, submergedVolume);
            }
        }

        protected virtual void OnTriggerEnter(Collider other)
        {
            if (other.CompareTag(WaterVolume.TAG))
            {
                this.water = other.GetComponent<WaterVolume>();
                if (this.voxels == null)
                {
                    this.voxels = this.CutIntoVoxels();
                }
            }
        }

        protected virtual void OnTriggerExit(Collider other)
        {
            if (other.CompareTag(WaterVolume.TAG))
            {
                this.water = null;
            }
        }

        protected virtual void OnDrawGizmos()
        {
            if (this.voxels != null)
            {
                for (int i = 0; i < this.voxels.Length; i++)
                {
                    Gizmos.color = Color.magenta - new Color(0f, 0f, 0f, 0.75f);
                    Gizmos.DrawCube(this.transform.TransformPoint(this.voxels[i]), this.voxelSize * 0.8f);
                }
            }
        }

        private Vector3 CalculateMaxBuoyancyForce()
        {
            float objectVolume = this.rigidbody.mass  / this.density;
			Vector3 maxBuoyancyForce = this.water.Density * objectVolume * -Physics.gravity;

            return maxBuoyancyForce;
        }

		private Vector3 CalculateBuoyancyTorque()
		{
			//计算浮力力矩
			//TODO  计算入水体积
			float waterVolume = CalculateWaterVolume();
			Vector3 r = new Vector3(0, 0, 0); //质心到入水中心的向量
			Matrix4x4 rotaionMatrix = this.transform.localToWorldMatrix;// TODO 还不知道是什么东西
			Vector3 BuoyancyForce = CalculateMaxBuoyancyForce();
			Vector3 mg = this.water.Density * waterVolume * Physics.gravity;
			Vector3 tempVector = -mg.y > BuoyancyForce.y ? -mg : BuoyancyForce;
			Vector3 BuoyancyTorque = rotaionMatrix.MultiplyVector(Matrix4x4.Transpose(rotaionMatrix).MultiplyVector(Vector3.Cross(tempVector, r)) * ParamTb);
			return BuoyancyTorque;
		}

		private float CalculateWaterVolume()
		{
			//TODO  计算入水体积

			return 0f;
		}

		private Vector3[] CutIntoVoxels()
        {
			//切割几何体成体积像素
            Quaternion initialRotation = this.transform.rotation;
            this.transform.rotation = Quaternion.identity;

            Bounds bounds = this.collider.bounds;
            this.voxelSize.x = bounds.size.x * this.normalizedVoxelSize;
            this.voxelSize.y = bounds.size.y * this.normalizedVoxelSize;
            this.voxelSize.z = bounds.size.z * this.normalizedVoxelSize;
            int voxelsCountForEachAxis = Mathf.RoundToInt(1f / this.normalizedVoxelSize);
            List<Vector3> voxels = new List<Vector3>(voxelsCountForEachAxis * voxelsCountForEachAxis * voxelsCountForEachAxis);

            for (int i = 0; i < voxelsCountForEachAxis; i++)
            {
                for (int j = 0; j < voxelsCountForEachAxis; j++)
                {
                    for (int k = 0; k < voxelsCountForEachAxis; k++)
                    {
                        float pX = bounds.min.x + this.voxelSize.x * (0.5f + i);
                        float pY = bounds.min.y + this.voxelSize.y * (0.5f + j);
                        float pZ = bounds.min.z + this.voxelSize.z * (0.5f + k);

                        Vector3 point = new Vector3(pX, pY, pZ);
                        if (ColliderUtils.IsPointInsideCollider(point, this.collider, ref bounds))
                        {
                            voxels.Add(this.transform.InverseTransformPoint(point));
                        }
                    }
                }
            }

            this.transform.rotation = initialRotation;

            return voxels.ToArray();
        }
    }
}