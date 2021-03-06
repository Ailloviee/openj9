################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
################################################################################

add_tracegen(j9codertvm.tdf)
add_library(j9codert_vm STATIC 
	cache.c
	cnathelp.cpp
	CodertVMHelpers.cpp
	decomp.cpp
	dlt.c
	fsdcsup.c
	jitavl.c
	jithash.cpp
	jsr292.c
	jswalk.c
	thunkcrt.c

	${CMAKE_CURRENT_BINARY_DIR}/ut_j9codertvm.c
)

target_include_directories(j9codert_vm
    PRIVATE
    .
	${CMAKE_CURRENT_BINARY_DIR}
    ${j9vm_SOURCE_DIR}/gc_include
    ${omr_SOURCE_DIR}/gc/include
	${j9vm_SOURCE_DIR}/compiler/trj9/runtime/
)

if(OMR_ARCH_X86)
  add_custom_command(
	  OUTPUT xnathelp.s
	  DEPENDS xnathelp.m4 run_constgen
	  COMMAND m4 -I${CMAKE_CURRENT_SOURCE_DIR}/../oti ${CMAKE_CURRENT_SOURCE_DIR}/xnathelp.m4 > xnathelp.s
  )

  # conditionalize this in the future
  target_sources(j9codert_vm PRIVATE
	  xnathelp.s
  )
elseif(OMR_ARCH_POWER)
  add_custom_command(
	  OUTPUT pnathelp.s
	  DEPENDS pnathelp.m4 run_constgen
	  COMMAND m4 -I${CMAKE_CURRENT_SOURCE_DIR}/../oti ${CMAKE_CURRENT_SOURCE_DIR}/pnathelp.m4 > pnathelp.s
  )

  # conditionalize this in the future
  target_sources(j9codert_vm PRIVATE
	  pnathelp.s
  )
endif()


if(J9VM_JIT_RUNTIME_INSTRUMENTATION)
  target_sources(j9codert_vm PRIVATE runtimeInstrumentation.c)
endif()

target_link_libraries(j9codert_vm
	PRIVATE
		j9vm_interface
		j9vm_compiler_defines
)
