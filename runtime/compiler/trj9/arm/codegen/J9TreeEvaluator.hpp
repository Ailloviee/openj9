/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef J9_ARM_TREE_EVALUATOR_INCL
#define J9_ARM_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_TREE_EVALUATOR_CONNECTOR
#define J9_TREE_EVALUATOR_CONNECTOR
namespace J9 { namespace ARM { class TreeEvaluator; } }
namespace J9 { typedef J9::ARM::TreeEvaluator TreeEvaluatorConnector; }
#else
#error J9::ARM::TreeEvaluator expected to be a primary connector, but a J9 connector is already defined
#endif


#include "trj9/codegen/J9TreeEvaluator.hpp"  // include parent

namespace J9
{

namespace ARM
{

class OMR_EXTENSIBLE TreeEvaluator: public J9::TreeEvaluator
   {
   public:

   static void genWrtbarForArrayCopy(TR::Node *node, TR::Register *srcObjReg, TR::Register *dstObjReg, TR::CodeGenerator *cg);

   };

}

}

#endif
