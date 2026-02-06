/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "Beliefs.h"
#include "CreatureAIEnums.h"

namespace openblack::creature
{

/// A single learning episode - records context when an action was taken
/// and the feedback received (positive or negative reinforcement)
struct LearningEpisode
{
	ObjectAttributes context {};    ///< Attributes of the object/situation
	float feedback = 0.0f;          ///< Reinforcement received (-1 to 1)
	uint32_t timestamp = 0;         ///< When this episode occurred
	uint8_t actionTaken = 0;        ///< Which action was performed
	uint8_t desireIndex = 0;        ///< Which desire motivated this

	/// Check if context matches a specific attribute value
	bool MatchesAttribute(AttributeType type, uint8_t value) const
	{
		return context.GetAttribute(type) == value;
	}
};

/// A node in the decision tree
/// Can be either a decision node (tests an attribute) or a leaf (gives opinion)
struct DecisionTreeNode
{
	AttributeType splitAttribute = AttributeType::Type;  ///< Which attribute to test
	uint8_t splitValue = 0;                              ///< Value to compare against

	std::unique_ptr<DecisionTreeNode> leftChild;   ///< <= splitValue
	std::unique_ptr<DecisionTreeNode> rightChild;  ///< > splitValue

	float leafOpinion = 0.0f;     ///< Opinion value if this is a leaf
	uint32_t sampleCount = 0;    ///< How many episodes contributed to this node
	bool isLeaf = true;

	/// Evaluate this node given object attributes
	float Evaluate(const ObjectAttributes& attrs) const;

	/// Check if this node should be a leaf (not enough data or low entropy)
	bool ShouldBeLeaf(const std::vector<LearningEpisode*>& episodes) const;
};

/// A complete decision tree for one desire
/// Built using ID3 algorithm from learning episodes
struct DecisionTree
{
	static constexpr size_t k_MaxEpisodes = 100;
	static constexpr float k_MinEntropyForSplit = 0.1f;

	std::vector<LearningEpisode> episodes;
	std::unique_ptr<DecisionTreeNode> root;
	uint8_t desireIndex = 0;
	bool needsRebuild = true;

	/// Add a learning episode
	void AddEpisode(const LearningEpisode& episode);

	/// Rebuild tree from episodes using ID3 algorithm
	void Rebuild();

	/// Evaluate opinion of an object for this desire
	float GetOpinion(const ObjectAttributes& attrs) const;

	/// Clear all episodes and reset tree
	void Reset();

private:
	/// ID3: Calculate entropy of a set of episodes
	static float CalculateEntropy(const std::vector<LearningEpisode*>& episodes);

	/// ID3: Calculate information gain for splitting on an attribute
	static float CalculateInformationGain(const std::vector<LearningEpisode*>& episodes,
	                                      AttributeType attr, uint8_t splitValue);

	/// ID3: Find best attribute and value to split on
	static std::pair<AttributeType, uint8_t> FindBestSplit(
	    const std::vector<LearningEpisode*>& episodes);

	/// ID3: Build subtree recursively
	std::unique_ptr<DecisionTreeNode> BuildSubtree(std::vector<LearningEpisode*>& episodes,
	                                               int depth);
};

/// Collection of decision trees - one per desire per lesson type
/// Original size: 0x140 bytes
struct DecisionTreeCollection
{
	static constexpr size_t k_NumDesires = 40;
	static constexpr size_t k_NumLessonTypes = static_cast<size_t>(LessonType::_COUNT);

	/// Trees organized by [lessonType][desireIndex]
	std::array<std::array<DecisionTree, k_NumDesires>, k_NumLessonTypes> trees;

	/// Initialize all trees
	void Initialize();

	/// Add a learning episode to the appropriate tree
	void AddEpisode(LessonType lessonType, uint8_t desireIndex,
	                const LearningEpisode& episode);

	/// Get opinion for a desire about an object
	float GetOpinion(uint8_t desireIndex, const ObjectAttributes& attrs) const;

	/// Rebuild trees that need it
	void RebuildDirtyTrees();

	/// Get tree for direct manipulation
	DecisionTree& GetTree(LessonType lessonType, uint8_t desireIndex)
	{
		return trees[static_cast<size_t>(lessonType)][desireIndex];
	}
};

} // namespace openblack::creature
