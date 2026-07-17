#include "InstancedPropRenderer.h"

#include "StageProp.h"
#include "../../Scene/SceneManager.h"
#include "../../Culling/CullingManager.h"
#include "../../Debug/DebugFlags/DebugFlags.h"

void InstancedPropRenderer::DrawLit()
{
	DrawBatched(false);
}

void InstancedPropRenderer::GenerateDepthMapFromLight()
{
	DrawBatched(true);
}

void InstancedPropRenderer::DrawBatched(bool _isShadowPass)
{
	// 1モデルぶんのまとめ情報(そのモデルのワーク＋各インスタンスのワールド行列)
	struct Batch
	{
		std::shared_ptr<KdModelWork> spWork;
		std::vector<Math::Matrix>	 worlds;
	};

	// 同じモデル(KdModelData)ごとにまとめる。キーはモデルデータの実体アドレス
	// (StagePropは種類ごとにKdAssetsのキャッシュを共有するので、同じ家なら同じポインタになる)
	std::unordered_map<const KdModelData*, Batch> batches;

	const bool useInstancing = DebugFlags::Instance().Get(U8("カリング/インスタンシング"), true);

	for (auto& obj : SceneManager::Instance().GetObjList())
	{
		if (!obj) { continue; }

		std::shared_ptr<StageProp> spProp = std::dynamic_pointer_cast<StageProp>(obj);
		if (!spProp) { continue; }

		const std::shared_ptr<KdModelWork>& spWork = spProp->GetModelWork();
		if (!spWork || !spWork->IsEnable()) { continue; }

		// カリング：StagePropが個別に描いていた時と同じ判定
		//  影は画面外の物でも影が画面に落ちうるので距離のみ、通常描画は視錐台＋距離
		const DirectX::BoundingSphere wbs = spProp->WorldBoundingSphere();
		const bool visible = _isShadowPass
			? CullingManager::Instance().IsInRange(wbs)
			: CullingManager::Instance().IsVisible(wbs);
		if (!visible) { continue; }

		// インスタンシングを切っている場合は、その場で1軒ずつ普通に描く(比較・保険用)
		if (!useInstancing)
		{
			KdShaderManager::Instance().m_StandardShader.DrawModel(*spWork, spProp->GetMatrix());
			continue;
		}

		const KdModelData* pKey = spWork->GetData().get();
		if (pKey == nullptr) { continue; }

		Batch& b = batches[pKey];
		if (!b.spWork)
		{
			b.spWork = spWork;
		}
		b.worlds.push_back(spProp->GetMatrix());
	}

	// モデルごとに1回ずつまとめて描く
	for (auto& kv : batches)
	{
		Batch& b = kv.second;
		if (!b.spWork || b.worlds.empty()) { continue; }

		KdShaderManager::Instance().m_StandardShader.DrawModelInstanced(*b.spWork, b.worlds);
	}
}
