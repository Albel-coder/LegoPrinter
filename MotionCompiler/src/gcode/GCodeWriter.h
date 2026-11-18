#pragma once

#include "../model/ImageProfile.h"

#include <optional>
#include <string>
#include <vector>

struct GCodeConfig {
    // Скорости (мм/мин)
    double feedDraw = 8000.0;
    double feedTravel = 8000.0;

    // Серво Z
    double zUpAngle = 2.0;
    double zDownAngle = 0.0;
    int zUpDwellMs = 200;
    int zDownDwellMs = 200;
    std::string zSyncCmd = "G4 P0";
    std::string zSyncAfterCmd = "G4 P0";
    int zDownExtraMs = 100;
    std::optional<double> zFeed;

    // Порог "не поднимать перо" между контурами (мм)
    double minHopMm = 0.0;

    // Компенсация люфта
    double backlashXMm = 0.0;
    double backlashYMm = 0.0;
    double initialPosX = 0.0;
    double initialPosY = 0.0;
    bool emitG92Start = true;

    // Только для шапки
    double widthMm = 0.0;
    double heightMm = 0.0;
    double rdpEpsilonMm = 1.0;
};

class GCodeWriter {
public:
    explicit GCodeWriter(GCodeConfig config = {});

    bool write(const ImageProfile& strokes, const std::string& filename) const;

    struct ZVerification {
        std::vector<std::string> errors;
        int ups = 0;
        int downs = 0;
        int g1 = 0;
    };

    static ZVerification verifyZ(const std::vector<std::string>& lines,
        double zUpAngle,
        double zDownAngle);

private:
    GCodeConfig config;
};
