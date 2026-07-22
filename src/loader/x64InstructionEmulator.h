#ifndef KYTY_LOADER_X64_INSTRUCTION_EMULATOR_H_
#define KYTY_LOADER_X64_INSTRUCTION_EMULATOR_H_

#include <cstdint>

namespace Loader::X64InstructionEmulator {

[[nodiscard]] bool TryEmulate(void* native_context);

// Soft-skip a guest access to the null page (failed asset loads / null object writes).
// Advances RIP past the faulting instruction when a length can be decoded.
[[nodiscard]] bool TrySkipNullPageAccess(void* native_context, uint64_t access_vaddr);

} // namespace Loader::X64InstructionEmulator

#endif /* KYTY_LOADER_X64_INSTRUCTION_EMULATOR_H_ */
