// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project


// SPDX-License-Identifier: GPL-2.0-or-later


//


// PS4 libSceDiscMap HLE, ported into KytyPlus's LIB_FUNC framework.


// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are


// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs


// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.





#include "libs/libs.h"


#include "loader/symbolDatabase.h"

#include "libs/errno.h"





#include <common/abi.h>





namespace Libs {





namespace LibDiscMap {


LIB_VERSION("libSceDiscMap", 1, "libSceDiscMap", 1, 1);






static int KYTY_SYSV_ABI sceDiscMapGetPackageSize() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceDiscMapIsRequestOnHDD() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI Func_7C980FFB0AA27E7A() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI Func_8A828CAEE7EDD5E9() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI Func_E7EBCE96E92F91F8() {


	PRINT_NAME();


	return OK; // STUBBED


}







LIB_DEFINE(InitDiscMap_1) {


	LIB_FUNC("fl1eoDnwQ4s", LibDiscMap::sceDiscMapGetPackageSize);


	LIB_FUNC("lbQKqsERhtE", LibDiscMap::sceDiscMapIsRequestOnHDD);


	LIB_FUNC("fJgP+wqifno", LibDiscMap::Func_7C980FFB0AA27E7A);


	LIB_FUNC("ioKMruft1ek", LibDiscMap::Func_8A828CAEE7EDD5E9);


	LIB_FUNC("5+vOlukvkfg", LibDiscMap::Func_E7EBCE96E92F91F8);


}


} // namespace LibDiscMap





} // namespace Libs


