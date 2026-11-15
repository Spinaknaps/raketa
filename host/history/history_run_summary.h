#pragma once

#include "run_metadata.h"

struct HistoryRunSummary {
    qlonglong id;
    RunMetadata metadata;
    QString outcome;
    QString detail;
};
