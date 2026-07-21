#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_OBJECTS_LABEL_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_OBJECTS_LABEL_H_

#include "common/abi.h"
#include "common/common.h"

namespace Libs::Graphics {

struct Label;
class CommandBuffer;
struct GraphicContext;

void LabelInit();

constexpr int LABEL_ARGS_MAX = 5;
using LabelCallback          = bool (*)(const uint64_t* args);

Label* LabelCreate(LabelCallback callback_1, LabelCallback callback_2, const uint64_t* args);
void   LabelDelete(Label& label);
void   LabelSet(CommandBuffer& buffer, Label& label);
void   LabelDrain();
[[nodiscard]] bool LabelInCallback() noexcept;

} // namespace Libs::Graphics

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_OBJECTS_LABEL_H_ */
