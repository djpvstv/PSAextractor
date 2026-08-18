#include "pac/arc_types.hpp"

namespace psax {

const char* arc_file_type_name(ArcFileType t) {
    switch (t) {
        case ArcFileType::None:          return "None";
        case ArcFileType::MiscData:      return "MiscData";
        case ArcFileType::ModelData:     return "ModelData";
        case ArcFileType::TextureData:   return "TextureData";
        case ArcFileType::AnimationData: return "AnimationData";
        case ArcFileType::SceneData:     return "SceneData";
        case ArcFileType::Type6:         return "Type6";
        case ArcFileType::EffectData:    return "EffectData";
        case ArcFileType::Type8:         return "Type8";
    }
    return "Unknown";
}

} // namespace psax
