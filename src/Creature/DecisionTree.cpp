/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

// DecisionTree.cpp - ID3 Decision Tree Learning Algorithm
// Implements Quinlan's ID3 algorithm for building decision trees from learning episodes
// Original designer: Richard Evans (Lionhead Studios, 2001)
//
// The creature uses these trees to form opinions about objects based on past experience.
// When the player strokes/slaps the creature while it's interacting with an object,
// the episode is recorded with the object's attributes and the feedback.
// ID3 then builds trees that predict what feedback would be received for similar objects.

#include "DecisionTree.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace openblack::creature
{

//=============================================================================
// DecisionTreeNode Implementation
//=============================================================================

float DecisionTreeNode::Evaluate(const ObjectAttributes& attrs) const
{
	// Leaf nodes return their opinion directly
	if (isLeaf)
	{
		return leafOpinion;
	}

	// Internal nodes test an attribute and recurse
	uint8_t value = attrs.GetAttribute(splitAttribute);

	if (value <= splitValue)
	{
		// Go left for values <= split point
		if (leftChild)
		{
			return leftChild->Evaluate(attrs);
		}
		return leafOpinion; // Fallback if child missing
	}
	else
	{
		// Go right for values > split point
		if (rightChild)
		{
			return rightChild->Evaluate(attrs);
		}
		return leafOpinion; // Fallback if child missing
	}
}

bool DecisionTreeNode::ShouldBeLeaf(const std::vector<LearningEpisode*>& episodes) const
{
	// A node should be a leaf if:
	// 1. Not enough episodes to split meaningfully
	if (episodes.size() < 3)
	{
		return true;
	}

	// 2. All episodes have the same feedback sign (pure node)
	bool hasPositive = false;
	bool hasNegative = false;
	for (const auto* ep : episodes)
	{
		if (ep->feedback > 0.0f)
		{
			hasPositive = true;
		}
		else
		{
			hasNegative = true;
		}
		if (hasPositive && hasNegative)
		{
			break;
		}
	}

	// Pure node - all same class
	if (!hasPositive || !hasNegative)
	{
		return true;
	}

	return false;
}

//=============================================================================
// DecisionTree Implementation - ID3 Algorithm
//=============================================================================

void DecisionTree::AddEpisode(const LearningEpisode& episode)
{
	// Add episode, enforcing max capacity
	if (episodes.size() >= k_MaxEpisodes)
	{
		// Remove oldest episode (FIFO)
		episodes.erase(episodes.begin());
	}

	episodes.push_back(episode);
	needsRebuild = true;
}

void DecisionTree::Rebuild()
{
	if (episodes.empty())
	{
		// No data - create trivial tree with neutral opinion
		root = std::make_unique<DecisionTreeNode>();
		root->isLeaf = true;
		root->leafOpinion = 0.0f;
		root->sampleCount = 0;
		needsRebuild = false;
		return;
	}

	// Create pointer vector for recursive building
	std::vector<LearningEpisode*> episodePtrs;
	episodePtrs.reserve(episodes.size());
	for (auto& ep : episodes)
	{
		episodePtrs.push_back(&ep);
	}

	// Build tree using ID3 algorithm
	root = BuildSubtree(episodePtrs, 0);
	needsRebuild = false;
}

float DecisionTree::GetOpinion(const ObjectAttributes& attrs) const
{
	if (!root)
	{
		return 0.0f; // Neutral if no tree
	}

	return root->Evaluate(attrs);
}

void DecisionTree::Reset()
{
	episodes.clear();
	root.reset();
	needsRebuild = true;
}

//=============================================================================
// ID3 Core Algorithm
//=============================================================================

float DecisionTree::CalculateEntropy(const std::vector<LearningEpisode*>& episodes)
{
	// Entropy measures the impurity of a set
	// H(S) = -p+ * log2(p+) - p- * log2(p-)
	// where p+ is proportion of positive feedback, p- is proportion of negative

	if (episodes.empty())
	{
		return 0.0f;
	}

	// Count positive and negative feedback episodes
	size_t positiveCount = 0;
	size_t negativeCount = 0;

	for (const auto* ep : episodes)
	{
		if (ep->feedback > 0.0f)
		{
			++positiveCount;
		}
		else
		{
			++negativeCount;
		}
	}

	// Handle pure sets (all same class)
	if (positiveCount == 0 || negativeCount == 0)
	{
		return 0.0f; // Pure set has zero entropy
	}

	// Calculate proportions
	const float total = static_cast<float>(episodes.size());
	const float pPos = static_cast<float>(positiveCount) / total;
	const float pNeg = static_cast<float>(negativeCount) / total;

	// Calculate entropy: H = -p+ * log2(p+) - p- * log2(p-)
	// Using change of base: log2(x) = ln(x) / ln(2)
	constexpr float kLog2 = 0.693147180559945f; // ln(2)

	const float entropy = -(pPos * std::log(pPos) / kLog2) - (pNeg * std::log(pNeg) / kLog2);

	return entropy;
}

float DecisionTree::CalculateInformationGain(const std::vector<LearningEpisode*>& episodes,
                                              AttributeType attr, uint8_t splitValue)
{
	// Information Gain = H(parent) - weighted average H(children)
	// IG(S, A=v) = H(S) - [|S_left|/|S| * H(S_left) + |S_right|/|S| * H(S_right)]

	if (episodes.empty())
	{
		return 0.0f;
	}

	// Split episodes by attribute value
	std::vector<LearningEpisode*> leftEpisodes;
	std::vector<LearningEpisode*> rightEpisodes;

	for (auto* ep : episodes)
	{
		if (ep->context.GetAttribute(attr) <= splitValue)
		{
			leftEpisodes.push_back(ep);
		}
		else
		{
			rightEpisodes.push_back(ep);
		}
	}

	// If split doesn't actually split, no gain
	if (leftEpisodes.empty() || rightEpisodes.empty())
	{
		return 0.0f;
	}

	// Calculate entropies
	const float parentEntropy = CalculateEntropy(episodes);
	const float leftEntropy = CalculateEntropy(leftEpisodes);
	const float rightEntropy = CalculateEntropy(rightEpisodes);

	// Calculate weighted average of children
	const float total = static_cast<float>(episodes.size());
	const float leftWeight = static_cast<float>(leftEpisodes.size()) / total;
	const float rightWeight = static_cast<float>(rightEpisodes.size()) / total;

	const float childrenEntropy = leftWeight * leftEntropy + rightWeight * rightEntropy;

	// Information gain is reduction in entropy
	return parentEntropy - childrenEntropy;
}

std::pair<AttributeType, uint8_t> DecisionTree::FindBestSplit(
    const std::vector<LearningEpisode*>& episodes)
{
	// Try each attribute and each possible split value
	// Return the (attribute, value) pair with highest information gain

	AttributeType bestAttribute = AttributeType::Type;
	uint8_t bestValue = 0;
	float bestGain = -1.0f;

	// Iterate through all attribute types
	const uint8_t numAttributes = static_cast<uint8_t>(AttributeType::_COUNT);

	for (uint8_t attrIdx = 0; attrIdx < numAttributes; ++attrIdx)
	{
		AttributeType attr = static_cast<AttributeType>(attrIdx);

		// Collect unique values for this attribute across all episodes
		std::set<uint8_t> uniqueValues;
		for (const auto* ep : episodes)
		{
			uniqueValues.insert(ep->context.GetAttribute(attr));
		}

		// Try each unique value as a split point
		for (uint8_t value : uniqueValues)
		{
			float gain = CalculateInformationGain(episodes, attr, value);

			if (gain > bestGain)
			{
				bestGain = gain;
				bestAttribute = attr;
				bestValue = value;
			}
		}
	}

	return {bestAttribute, bestValue};
}

std::unique_ptr<DecisionTreeNode> DecisionTree::BuildSubtree(
    std::vector<LearningEpisode*>& episodes, int depth)
{
	// Create node
	auto node = std::make_unique<DecisionTreeNode>();
	node->sampleCount = static_cast<uint32_t>(episodes.size());

	// Calculate mean feedback for this node (used as opinion if leaf)
	float feedbackSum = 0.0f;
	for (const auto* ep : episodes)
	{
		feedbackSum += ep->feedback;
	}
	node->leafOpinion = episodes.empty() ? 0.0f : feedbackSum / static_cast<float>(episodes.size());

	// Check stopping conditions
	constexpr int k_MaxDepth = 10;

	if (episodes.size() < 3 ||          // Not enough data to split meaningfully
	    depth >= k_MaxDepth ||           // Max depth reached
	    node->ShouldBeLeaf(episodes))    // Pure node or other stopping condition
	{
		node->isLeaf = true;
		return node;
	}

	// Check entropy - if already low enough, make leaf
	float entropy = CalculateEntropy(episodes);
	if (entropy < k_MinEntropyForSplit)
	{
		node->isLeaf = true;
		return node;
	}

	// Find best attribute and value to split on
	auto [bestAttr, bestValue] = FindBestSplit(episodes);

	// Split episodes into left and right subsets
	std::vector<LearningEpisode*> leftEpisodes;
	std::vector<LearningEpisode*> rightEpisodes;

	for (auto* ep : episodes)
	{
		if (ep->context.GetAttribute(bestAttr) <= bestValue)
		{
			leftEpisodes.push_back(ep);
		}
		else
		{
			rightEpisodes.push_back(ep);
		}
	}

	// If split doesn't actually split the data, make leaf
	if (leftEpisodes.empty() || rightEpisodes.empty())
	{
		node->isLeaf = true;
		return node;
	}

	// Create internal node
	node->isLeaf = false;
	node->splitAttribute = bestAttr;
	node->splitValue = bestValue;

	// Recursively build children
	node->leftChild = BuildSubtree(leftEpisodes, depth + 1);
	node->rightChild = BuildSubtree(rightEpisodes, depth + 1);

	return node;
}

//=============================================================================
// DecisionTreeCollection Implementation
//=============================================================================

void DecisionTreeCollection::Initialize()
{
	// Initialize all trees with their desire indices
	for (size_t lessonType = 0; lessonType < k_NumLessonTypes; ++lessonType)
	{
		for (size_t desireIdx = 0; desireIdx < k_NumDesires; ++desireIdx)
		{
			trees[lessonType][desireIdx].desireIndex = static_cast<uint8_t>(desireIdx);
			trees[lessonType][desireIdx].Reset();
		}
	}
}

void DecisionTreeCollection::AddEpisode(LessonType lessonType, uint8_t desireIndex,
                                         const LearningEpisode& episode)
{
	if (desireIndex >= k_NumDesires)
	{
		return;
	}

	auto lessonIdx = static_cast<size_t>(lessonType);
	if (lessonIdx >= k_NumLessonTypes)
	{
		return;
	}

	trees[lessonIdx][desireIndex].AddEpisode(episode);
}

float DecisionTreeCollection::GetOpinion(uint8_t desireIndex, const ObjectAttributes& attrs) const
{
	if (desireIndex >= k_NumDesires)
	{
		return 0.0f;
	}

	// Aggregate opinion across lesson types
	// The original game weighted different lesson types differently
	// For now, we average the most relevant opinion trees:
	// - IncreaseOpinion (4) and DecreaseOpinion (5) directly affect opinions
	// - Other lesson types affect desires/sources/actions

	float opinionSum = 0.0f;
	float weightSum = 0.0f;

	// Opinion-related lesson types have higher weight
	constexpr std::array<float, k_NumLessonTypes> weights = {
	    0.5f,  // IncreaseSource
	    0.5f,  // DecreaseSource
	    0.3f,  // IncreaseDesire
	    0.3f,  // DecreaseDesire
	    1.0f,  // IncreaseOpinion - primary
	    1.0f,  // DecreaseOpinion - primary
	    0.2f,  // LearnNormalAction
	    0.2f   // LearnMagicAction
	};

	for (size_t lessonIdx = 0; lessonIdx < k_NumLessonTypes; ++lessonIdx)
	{
		const DecisionTree& tree = trees[lessonIdx][desireIndex];
		if (!tree.episodes.empty())
		{
			float opinion = tree.GetOpinion(attrs);
			float weight = weights[lessonIdx];

			// Adjust weight based on sample count (more samples = more confident)
			if (tree.root)
			{
				weight *= std::min(1.0f, tree.root->sampleCount / 20.0f);
			}

			opinionSum += opinion * weight;
			weightSum += weight;
		}
	}

	if (weightSum < 0.001f)
	{
		return 0.0f; // No data - neutral opinion
	}

	return opinionSum / weightSum;
}

void DecisionTreeCollection::RebuildDirtyTrees()
{
	for (auto& lessonTrees : trees)
	{
		for (auto& tree : lessonTrees)
		{
			if (tree.needsRebuild && !tree.episodes.empty())
			{
				tree.Rebuild();
			}
		}
	}
}

} // namespace openblack::creature
