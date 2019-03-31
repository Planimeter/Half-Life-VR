
#pragma once

#include <vector>
#include <unordered_map>

namespace reactphysics3d {
	class CollisionWorld;
	class DynamicsWorld;
	class BoxShape;
	class CollisionBody;
	class TriangleVertexArray;
	class TriangleMesh;
	class ConcaveMeshShape;
	class ProxyShape;
	struct Vector3;
};

class CWorld;
class CBaseEntity;

class VRPhysicsHelper
{
public:
	VRPhysicsHelper();
	~VRPhysicsHelper();

	static void CreateInstance();
	static void DestroyInstance();
	static VRPhysicsHelper& Instance();

	// Called by engine every frame
	inline void StartFrame()
	{
		// Make sure we have an up-to-date model of the world!
		CheckWorld();
	}

	bool CheckIfLineIsBlocked(const Vector & hlPos1, const Vector &hlPos2);
	bool CheckIfLineIsBlocked(const Vector & pos1, const Vector & pos2, Vector & result);

	Vector RotateVectorInline(const Vector &vecToRotate, const Vector &vecAngles, const Vector &vecOffset = Vector(), const bool reverse = false);
	void RotateVector(Vector &vecToRotate, const Vector &vecAngles, const Vector &vecOffset = Vector(), const bool reverse = false);
	Vector AngularVelocityToLinearVelocity(const Vector & avelocity, const Vector & pos);

	void TraceLine(const Vector &vecStart, const Vector &vecEnd, edict_t* pentIgnore, TraceResult *ptr);

	bool ModelIntersectsBBox(CBaseEntity *pModel, const Vector& bboxCenter, const Vector& bboxMins, const Vector& bboxMaxs);
	bool ModelIntersectsWorld(CBaseEntity *pModel);
	bool ModelsIntersect(CBaseEntity *pModel1, CBaseEntity *pModel2);

	/*
	bool RotatedBBoxIntersectsWorld(const Vector & bboxCenter, const Vector & bboxAngles, const Vector & bboxMins, const Vector & bboxMaxs);
	bool RotatedBBoxesIntersect(
		const Vector & bbox1Center, const Vector & bbox1Angles, const Vector & bbox1Mins, const Vector & bbox1Maxs,
		const Vector & bbox2Center, const Vector & bbox2Angles, const Vector & bbox2Mins, const Vector & bbox2Maxs);
	*/

	class BSPModelData
	{
	public:
		~BSPModelData();

		void CreateData(reactphysics3d::CollisionWorld* collisionWorld);
		void DeleteData();
		bool HasData() { return m_hasData; }

		std::vector<struct reactphysics3d::Vector3>		m_vertices;
		std::vector<int32_t>							m_indices;

		reactphysics3d::TriangleVertexArray*			m_triangleVertexArray{ nullptr };
		reactphysics3d::TriangleMesh*					m_triangleMesh{ nullptr };
		reactphysics3d::ConcaveMeshShape*				m_concaveMeshShape{ nullptr };
		reactphysics3d::ProxyShape*						m_proxyShape{ nullptr };
		reactphysics3d::CollisionBody*					m_collisionBody{ nullptr };

		reactphysics3d::CollisionWorld*					m_collisionWorld{ nullptr };

		bool											m_hasData{ false };
	};

private:
	bool CheckWorld();

	void InitPhysicsWorld();

	bool GetPhysicsMapDataFromFile(const std::string& physicsMapDataFilePath);
	void StorePhysicsMapDataToFile(const std::string& physicsMapDataFilePath);
	void GetPhysicsMapDataFromModel();

	std::string										m_currentMapName;
	std::unordered_map<std::string, BSPModelData>	m_bspModelData;

	const struct model_s*							m_hlWorldModel{ nullptr };
	reactphysics3d::CollisionWorld*					m_collisionWorld{ nullptr };

	reactphysics3d::CollisionBody*					m_bboxBody{ nullptr };
	reactphysics3d::BoxShape*						m_bboxShape{ nullptr };
	reactphysics3d::ProxyShape*						m_bboxProxyShape{ nullptr };


	static VRPhysicsHelper*		m_instance;
};
