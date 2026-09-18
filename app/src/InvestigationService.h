#pragma once

#include "Storage.h"

#include <string>
#include <vector>

namespace continuum
{

struct ConflictViewRecord
{
    std::string id;
    std::string type;
    std::string severity;
    std::string title;
    std::string description;
    std::string objectId;
    std::string propertyName;
    std::string status;
    std::string resolution;
    std::string resolutionNote;
    std::string detectedAt;
    std::string claimA;
    std::string sourceA;
    std::string claimB;
    std::string sourceB;
};

struct ChangeReviewViewRecord
{
    std::string id;
    std::string fileId;
    std::string fileName;
    std::string changeType;
    std::string status;
    std::string detectedAt;
    std::string previousContent;
    std::string currentContent;
    int affectedEvidence = 0;
    int affectedObjects = 0;
};

struct RelationViewRecord
{
    std::string id;
    std::string sourceId;
    std::string sourceTitle;
    std::string targetId;
    std::string targetTitle;
    std::string type;
    std::string status;
    std::string note;
};

class InvestigationService final
{
public:
    explicit InvestigationService(Database& database);

    std::vector<ConflictViewRecord> ListConflicts(
        const std::string& status,
        const std::string& type,
        int limit = 500
    ) const;

    StorageStatus DetectConflicts(
        const std::string& actor = "ui"
    );

    StorageStatus ResolveConflict(
        const std::string& id,
        const std::string& resolution,
        const std::string& note,
        const std::string& actor = "ui"
    );

    std::vector<ChangeReviewViewRecord> ListChanges(
        const std::string& status,
        int limit = 500
    ) const;

    StorageStatus ResolveChange(
        const std::string& id,
        const std::string& resolution,
        const std::string& note,
        const std::string& actor = "ui"
    );

    std::vector<RelationViewRecord> QueryRelations(
        const std::string& startObjectId,
        const std::string& targetObjectId,
        const std::string& relationType,
        int maximumDepth,
        bool includeInvalid
    ) const;

    StorageStatus SaveRelation(
        const std::string& sourceObjectId,
        const std::string& targetObjectId,
        const std::string& relationType,
        const std::string& note,
        const std::string& actor = "ui"
    );

private:
    Database& database_;
};

}
