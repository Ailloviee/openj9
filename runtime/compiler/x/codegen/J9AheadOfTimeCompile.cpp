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

#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "compile/AOTClassInfo.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CHTable.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/SharedCache.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/CodeCacheManager.hpp"

#define NON_HELPER   0x00

void J9::X86::AheadOfTimeCompile::processRelocations()
   {
   // calculate the amount of memory needed to hold the relocation data
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());

   if (TR::Compiler->target.is64Bit()
       && TR::CodeCacheManager::instance()->codeCacheConfig().needsMethodTrampolines()
       && _cg->getPicSlotCount())
      {
      _cg->addExternalRelocation(new (_cg->trHeapMemory()) TR::ExternalRelocation(NULL,
                                                                                 (uint8_t *)_cg->getPicSlotCount(),
                                                                                 TR_PicTrampolines, _cg),
                            __FILE__,
                            __LINE__,
                            NULL);
      }


   for (auto aotIterator = _cg->getExternalRelocationList().begin(); aotIterator != _cg->getExternalRelocationList().end(); ++aotIterator)
	  (*aotIterator)->addExternalRelocation(_cg);

   TR::IteratedExternalRelocation *r;
   for (r = self()->getAOTRelocationTargets().getFirst();
        r != NULL;
        r = r->getNext())
      {
      self()->addToSizeOfAOTRelocations(r->getSizeOfRelocationData());
      }
   // now allocate the memory  size of all iterated relocations + the header (total length field)
   if (self()->getSizeOfAOTRelocations() != 0)
      {
      uint8_t *relocationDataCursor = self()->setRelocationData(fej9->allocateRelocationData(self()->comp(), self()->getSizeOfAOTRelocations() + SIZEPOINTER));
      // set up the size for the region
      *(uint64_t *)relocationDataCursor = self()->getSizeOfAOTRelocations() + SIZEPOINTER;
      relocationDataCursor += SIZEPOINTER;
      // set up pointers for each iterated relocation and initialize header
      TR::IteratedExternalRelocation *s;
      for (s = self()->getAOTRelocationTargets().getFirst();
           s != 0;
           s = s->getNext())
         {
         s->setRelocationData(relocationDataCursor);
         s->initializeRelocation(_cg);
         relocationDataCursor += s->getSizeOfRelocationData();
         }
      }
   }

uintptr_t findCorrectInlinedSiteIndex(void *constantPool, TR::Compilation *comp, uintptr_t currentInlinedSiteIndex);

uint8_t *J9::X86::AheadOfTimeCompile::initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());
   TR_SharedCache *sharedCache = fej9->sharedCache();
   TR::Compilation * comp = _cg->comp();
   uintptrj_t numTrampolines;
   TR::LabelSymbol *label;
   TR_ResolvedMethod *resolvedMethod;
   TR_VirtualGuard *guard;
   uint8_t flags = 0;
   uint8_t *cursor = relocation->getRelocationData();
   // size of relocation goes first in all types
   *(uint16_t *)cursor = relocation->getSizeOfRelocationData();
   cursor += 2;
   uint8_t  wideOffsets = relocation->needsWideOffsets() ? RELOCATION_TYPE_WIDE_OFFSET : 0;
   *cursor++ = (uint8_t)relocation->getTargetKind();
   uint8_t *flagsCursor = cursor++;
   *flagsCursor = wideOffsets;
   uint32_t *wordAfterHeader = (uint32_t*)cursor;
#if defined(TR_HOST_64BIT)
   cursor += 4; // padding
#endif

   switch (relocation->getTargetKind())
      {
      case TR_HelperAddress:
         {
         *flagsCursor |= RELOCATION_TYPE_EIP_OFFSET; // set the eip relative relocation bit for references to code addresses
         }
         // deliberate fall-through
      case TR_AbsoluteHelperAddress:
         {
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();
         *wordAfterHeader = (uint32_t) tempSR->getReferenceNumber();
         cursor = (uint8_t*)wordAfterHeader;
         cursor += 4;
         //printf("TR_Helper relo helperIndex: %d\n", tempSR->getReferenceNumber());
         //printf("value at location: %d\n", *(uint32_t*)cursor);
         }
         break;

      case TR_GlobalValue:
         *(uintptrj_t*)cursor = (uintptr_t) relocation->getTargetAddress();
         cursor += SIZEPOINTER;
         break;

      case TR_MethodPointer:
         {

         TR::Node *aconstNode = (TR::Node *) relocation->getTargetAddress();
         uintptrj_t inlinedSiteIndex = (uintptrj_t)aconstNode->getInlinedSiteIndex();
         *(uintptrj_t *)cursor = inlinedSiteIndex;
         cursor += SIZEPOINTER;

         TR_OpaqueMethodBlock *j9method = (TR_OpaqueMethodBlock *) aconstNode->getAddress();
         TR_OpaqueClassBlock *j9class = fej9->getClassFromMethodBlock(j9method);

         void *loaderForClazz = fej9->getClassLoader(j9class);
         void *classChainIdentifyingLoaderForClazz = sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
         //traceMsg(comp(),"classChainIdentifyingLoaderForClazz %p\n", classChainIdentifyingLoaderForClazz);
         uintptrj_t classChainOffsetInSharedCache = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
         //traceMsg(comp(),"classChainOffsetInSharedCache %p\n", classChainOffsetInSharedCache);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = self()->emitClassChainOffset(cursor, j9class);

         uintptrj_t vTableOffset = (uintptrj_t) fej9->getInterpreterVTableSlot(j9method, j9class);
         *(uintptrj_t *)cursor = vTableOffset;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_ClassPointer:
         {

         TR::Node *aconstNode = (TR::Node *) relocation->getTargetAddress();
         uintptrj_t inlinedSiteIndex = (uintptrj_t)aconstNode->getInlinedSiteIndex();
         *(uintptrj_t *)cursor = inlinedSiteIndex;
         cursor += SIZEPOINTER;

         TR_OpaqueClassBlock *j9class = (TR_OpaqueClassBlock *) aconstNode->getAddress();

         void *loaderForClazz = fej9->getClassLoader(j9class);
         void *classChainIdentifyingLoaderForClazz = sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
         //traceMsg(comp(),"classChainIdentifyingLoaderForClazz %p\n", classChainIdentifyingLoaderForClazz);
         uintptrj_t classChainOffsetInSharedCache = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
         //traceMsg(comp(),"classChainOffsetInSharedCache %p\n", classChainOffsetInSharedCache);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = self()->emitClassChainOffset(cursor, j9class);
         }
         break;

      case TR_ArbitraryClassAddress:
         {
         // ExternalRelocation data is as expected for TR_ClassAddress
         auto symRef = (TR::SymbolReference *)relocation->getTargetAddress();
         auto sym = symRef->getSymbol()->castToStaticSymbol();
         auto j9class = (TR_OpaqueClassBlock *)sym->getStaticAddress();
         uintptr_t inlinedSiteIndex = findCorrectInlinedSiteIndex(
            symRef->getOwningMethod(comp)->constantPool(),
            comp,
            (uintptr_t)relocation->getTargetAddress2());

         // Data identifying the class is as though for TR_ClassPointer
         // (TR_RelocationRecordPointerBinaryTemplate)
         *(uintptrj_t *)cursor = inlinedSiteIndex;
         cursor += SIZEPOINTER;

         void *loaderForClazz = fej9->getClassLoader(j9class);
         void *classChainIdentifyingLoaderForClazz = sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
         uintptrj_t classChainOffsetInSharedCache = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = self()->emitClassChainOffset(cursor, j9class);
         }
         break;

      case TR_InlinedStaticMethodWithNopGuard:
      case TR_InlinedSpecialMethodWithNopGuard:
      case TR_InlinedVirtualMethodWithNopGuard:
      case TR_InlinedVirtualMethod:
      case TR_InlinedInterfaceMethodWithNopGuard:
      case TR_InlinedInterfaceMethod:
      case TR_InlinedHCRMethod:
         {
         guard = (TR_VirtualGuard *)relocation->getTargetAddress2();

         if (relocation->getTargetKind() == TR_InlinedHCRMethod)
            {
            // Setup flags field with type of method that needs to be validated at relocation time
            if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isStatic())
               flags = inlinedMethodIsStatic;
            if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isSpecial())
               flags = inlinedMethodIsSpecial;
            if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isVirtual())
               flags = inlinedMethodIsVirtual;
            }
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

         int32_t inlinedSiteIndex = guard->getCurrentInlinedSiteIndex();
         *(uintptrj_t *)cursor = (uintptrj_t)inlinedSiteIndex;
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)guard->getSymbolReference()->getOwningMethod(comp)->constantPool(); // record constant pool
         cursor += SIZEPOINTER;

         *(uintptrj_t*)cursor = (uintptrj_t)guard->getSymbolReference()->getCPIndex(); // record cpIndex
         cursor += SIZEPOINTER;

         if (relocation->getTargetKind() == TR_InlinedInterfaceMethodWithNopGuard ||
             relocation->getTargetKind() == TR_InlinedInterfaceMethod)
            {
            TR_InlinedCallSite *inlinedCallSite = &comp->getInlinedCallSite(inlinedSiteIndex);
            TR_AOTMethodInfo *aotMethodInfo = (TR_AOTMethodInfo *)inlinedCallSite->_methodInfo;
            resolvedMethod = aotMethodInfo->resolvedMethod;
            }
         else
            {
            resolvedMethod = guard->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod();
            }

         TR_OpaqueClassBlock *inlinedMethodClass = resolvedMethod->containingClass();
         void *romClass = (void *)fej9->getPersistentClassPointerFromClassPointer(inlinedMethodClass);
         void *romClassOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(romClass);

         *(uintptrj_t *)cursor = (uintptrj_t)romClassOffsetInSharedCache;
         cursor += SIZEPOINTER;

         if (relocation->getTargetKind() != TR_InlinedInterfaceMethod &&
             relocation->getTargetKind() != TR_InlinedVirtualMethod)
            {
            *(uintptrj_t *)cursor = (uintptrj_t)relocation->getTargetAddress(); // record Patch Destination Address
            cursor += SIZEPOINTER;
            }
         }
         break;

      case TR_ProfiledInlinedMethodRelocation:
      case TR_ProfiledClassGuardRelocation:
      case TR_ProfiledMethodGuardRelocation :
         {
         guard = (TR_VirtualGuard *)relocation->getTargetAddress2();

         int32_t inlinedSiteIndex = guard->getCurrentInlinedSiteIndex();
         *(uintptrj_t *)cursor = (uintptrj_t)inlinedSiteIndex;
         cursor += SIZEPOINTER;

         TR::SymbolReference *callSymRef = guard->getSymbolReference();
         TR_ResolvedMethod *owningMethod = callSymRef->getOwningMethod(comp);
         *(uintptrj_t *)cursor = (uintptrj_t)owningMethod->constantPool(); // record constant pool
         cursor += SIZEPOINTER;

         *(uintptrj_t*)cursor = (uintptrj_t)callSymRef->getCPIndex(); // record cpIndex
         cursor += SIZEPOINTER;

         TR_OpaqueClassBlock *inlinedCodeClass = guard->getThisClass();
         int32_t len;
         char *className = fej9->getClassNameChars(inlinedCodeClass, len);
         //traceMsg(comp(), "inlined site index is %d, inlined code class is %.*s\n", inlinedSiteIndex, len, className);

         void *romClass = (void *)fej9->getPersistentClassPointerFromClassPointer(inlinedCodeClass);
         void *romClassOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(romClass);
         traceMsg(comp, "class is %p, romclass is %p, offset is %p\n", inlinedCodeClass, romClass, romClassOffsetInSharedCache);
         *(uintptrj_t *)cursor = (uintptrj_t) romClassOffsetInSharedCache;
         cursor += SIZEPOINTER;

         void *loaderForClazz = fej9->getClassLoader(inlinedCodeClass);
         void *classChainIdentifyingLoaderForClazz = sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
         //traceMsg(comp(),"classChainIdentifyingLoaderForClazz %p\n", classChainIdentifyingLoaderForClazz);
         uintptrj_t classChainOffsetInSharedCache = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
         //traceMsg(comp(),"classChainOffsetInSharedCache %p\n", classChainOffsetInSharedCache);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = self()->emitClassChainOffset(cursor, inlinedCodeClass);

         uintptrj_t offset;
         TR::MethodSymbol *calleeSymbol = callSymRef->getSymbol()->castToMethodSymbol();
         if (!TR::Compiler->cls.isInterfaceClass(comp, inlinedCodeClass) && calleeSymbol->isInterface())
            {
            int32_t index = owningMethod->getResolvedInterfaceMethodOffset(inlinedCodeClass, callSymRef->getCPIndex());
            //traceMsg(comp(),"ProfiledGuard offset for interface is %d\n", index);
            offset = (uintptrj_t) index;
            if (index < 0)
               offset = (uintptrj_t) fej9->virtualCallOffsetToVTableSlot(index);
            //traceMsg(comp(),"ProfiledGuard offset for interface is %d\n", index);
            }
         else
            {
            offset = (int32_t) callSymRef->getOffset();
            //traceMsg(comp(),"ProfiledGuard offset for non-interface is %d\n", offset);
            offset = (uintptrj_t) fej9->virtualCallOffsetToVTableSlot((int32_t) callSymRef->getOffset());
            //traceMsg(comp(),"ProfiledGuard offset for non-interface is now %d\n", offset);
            }

         //traceMsg(comp(),"ProfiledGuard offset using %d\n", offset);
         *(uintptrj_t *)cursor =  offset;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_RelativeMethodAddress:
         {
         *flagsCursor |= RELOCATION_TYPE_EIP_OFFSET;  // set the relative relocation bit for references to code addresses
         }
         // deliberate fall-through
      case TR_ClassObject:
      case TR_MethodObject:
      //case TR_InterfaceObject: Shouldn't have branch that create inteface object for X86.
         {
         TR_ASSERT((relocation->getTargetKind() != TR_MethodObject), "relocation TR_MethodObject being created");
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();
         // next word is the address of the constant pool to which the index refers
         // index in the below stored constant pool that indicates the particular
         // relocation target

         *(uintptrj_t *)cursor = (uintptrj_t)relocation->getTargetAddress2(); // inlinedSiteIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)tempSR->getOwningMethod(comp)->constantPool();
         cursor += SIZEPOINTER;
         }
         break;
      case TR_JNIStaticTargetAddress:
      case TR_JNISpecialTargetAddress:
      case TR_JNIVirtualTargetAddress:
      case TR_StaticRamMethodConst:
      case TR_VirtualRamMethodConst:
      case TR_SpecialRamMethodConst:
         {
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();
         uintptr_t inlinedSiteIndex = (uintptr_t)relocation->getTargetAddress2();

         inlinedSiteIndex = findCorrectInlinedSiteIndex(tempSR->getOwningMethod(comp)->constantPool(), comp, inlinedSiteIndex);

         *(uintptrj_t *)cursor = inlinedSiteIndex; // inlinedSiteIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)tempSR->getOwningMethod(comp)->constantPool(); // constantPool
         cursor += SIZEPOINTER;

         uintptrj_t cpIndex=(uintptrj_t)tempSR->getCPIndex();
         *(uintptrj_t *)cursor =cpIndex;// cpIndex
         cursor += SIZEPOINTER;
         }
         break;
      case TR_ClassAddress:
         {
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();
         uintptr_t inlinedSiteIndex = (uintptrj_t)relocation->getTargetAddress2();

         inlinedSiteIndex = findCorrectInlinedSiteIndex(tempSR->getOwningMethod(comp)->constantPool(), comp, inlinedSiteIndex);

         *(uintptrj_t *)cursor = inlinedSiteIndex; // inlinedSiteIndex

         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)tempSR->getOwningMethod(comp)->constantPool(); // constantPool
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = tempSR->getCPIndex(); // cpIndex
         cursor += SIZEPOINTER;
         }
         break;
      case TR_DataAddress:
         {
         // next word is the address of the constant pool to which the index refers
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();
         uintptr_t inlinedSiteIndex = (uintptrj_t)relocation->getTargetAddress2();

         inlinedSiteIndex = findCorrectInlinedSiteIndex(tempSR->getOwningMethod(comp)->constantPool(), comp, inlinedSiteIndex);

         // relocation target
         *(uintptrj_t *)cursor = inlinedSiteIndex; // inlinedSiteIndex
         //*(uintptrj_t *)cursor = inlinedSiteIndex; //(uintptrj_t)relocation->getTargetAddress2(); // inlinedSiteIndex

         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)tempSR->getOwningMethod(comp)->constantPool(); // constantPool
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = tempSR->getCPIndex(); // cpIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = tempSR->getOffset(); // offset
         cursor += SIZEPOINTER;

         }
	 break;
      case TR_VerifyClassObjectForAlloc:
         {
         TR::SymbolReference * classSymRef = (TR::SymbolReference *) relocation->getTargetAddress();
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress2();
         TR::LabelSymbol *label = (TR::LabelSymbol *) recordInfo->data3;
         TR::Instruction *instr = (TR::Instruction *) recordInfo->data4;

         uint32_t branchOffset = (uint32_t) (label->getCodeLocation() - instr->getBinaryEncoding());

         *(uintptrj_t *)cursor = (uintptrj_t) recordInfo->data2; // inlined call site index
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t) classSymRef->getOwningMethod(comp)->constantPool();
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t) classSymRef->getCPIndex(); // cp Index
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t) branchOffset;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t) recordInfo->data1; // allocation size.
         cursor += SIZEPOINTER;
         }
         break;

      case TR_VerifyRefArrayForAlloc:
         {

         TR::SymbolReference * classSymRef = (TR::SymbolReference *) relocation->getTargetAddress();
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress2();
         TR::LabelSymbol *label = (TR::LabelSymbol *) recordInfo->data3;
         TR::Instruction *instr = (TR::Instruction *) recordInfo->data4;

         uint32_t branchOffset = (uint32_t) (label->getCodeLocation() - instr->getBinaryEncoding());

         *(uintptrj_t *)cursor = (uintptrj_t) recordInfo->data2; // inlined call site index
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t) classSymRef->getOwningMethod(comp)->constantPool();
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t) classSymRef->getCPIndex(); // cp Index
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t) branchOffset;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_AbsoluteMethodAddress:
      case TR_AbsoluteMethodAddressOrderedPair:
         break;
      case TR_BodyInfoAddress:
         break;

      case TR_ConstantPool:
      case TR_ConstantPoolOrderedPair:
      case TR_Trampolines:
      case TR_Thunks:
         {
         // Note: thunk relos should only be created for 64 bit
         *(uintptrj_t *)cursor = (uintptrj_t)relocation->getTargetAddress2(); // inlined site index
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)relocation->getTargetAddress(); // constantPool
         cursor += SIZEPOINTER;
         }
         break;

      case TR_PicTrampolines:
         {
         TR_ASSERT(TR::Compiler->target.is64Bit(), "TR_PicTrampolines not supported on 32-bit");
         numTrampolines = (uintptrj_t)relocation->getTargetAddress();
         *wordAfterHeader = numTrampolines;
         cursor = (uint8_t*)wordAfterHeader;
         cursor += 4;
         }
        break;

      case TR_CheckMethodEnter:
      case TR_CheckMethodExit:
         {
         *(uintptrj_t*)cursor = (uintptrj_t)relocation->getTargetAddress();
         cursor += SIZEPOINTER;
         }
        break;
     case TR_J2IThunks:
         {
         // Note: thunk relos should only be created for 64 bit
         // *(uintptrj_t *)cursor = (uintptrj_t)relocation->getTargetAddress2(); // inlined site index

         TR::Node *node = (TR::Node*)relocation->getTargetAddress();
         TR::SymbolReference *symRef = node->getSymbolReference();

         *(uintptrj_t *)cursor = (uintptrj_t)node->getInlinedSiteIndex();
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)symRef->getOwningMethod(comp)->constantPool(); // cp address
         cursor += SIZEPOINTER;


         *(uintptrj_t *)cursor = (uintptrj_t)symRef->getCPIndex(); // cp index
         cursor += SIZEPOINTER;

         break;
         }
      case TR_RamMethod:
         break;

      case TR_ValidateClass:
      case TR_ValidateInstanceField:
         {
         *(uintptrj_t*)cursor = (uintptrj_t) relocation->getTargetAddress(); // Inlined site index
         cursor += SIZEPOINTER;

         TR::AOTClassInfo *aotCI = (TR::AOTClassInfo*)relocation->getTargetAddress2();
         *(uintptrj_t*)cursor = (uintptrj_t) aotCI->_constantPool;
         cursor += SIZEPOINTER;

         *(uintptrj_t*)cursor = (uintptrj_t) aotCI->_cpIndex;
         cursor += SIZEPOINTER;

         void *classChainOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(aotCI->_classChain);
         *(uintptrj_t *)cursor = (uintptrj_t) classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_ValidateStaticField:
         {
         *(uintptrj_t*)cursor = (uintptrj_t) relocation->getTargetAddress(); // Inlined site index
         cursor += SIZEPOINTER;

         TR::AOTClassInfo *aotCI = (TR::AOTClassInfo*)relocation->getTargetAddress2();
         *(uintptrj_t*)cursor = (uintptrj_t) aotCI->_constantPool;
         cursor += SIZEPOINTER;

         *(uintptrj_t*)cursor = (uintptrj_t) aotCI->_cpIndex;
         cursor += SIZEPOINTER;

         void *romClass = (void *)fej9->getPersistentClassPointerFromClassPointer(aotCI->_clazz);
         void *romClassOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(romClass);
         *(uintptrj_t *)cursor = (uintptrj_t) romClassOffsetInSharedCache;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_ValidateArbitraryClass:
         {
         TR::AOTClassInfo *aotCI = (TR::AOTClassInfo*) relocation->getTargetAddress2();
         TR_OpaqueClassBlock *classToValidate = aotCI->_clazz;

         //store the classchain's offset for the classloader for the class
         void *loaderForClazzToValidate = fej9->getClassLoader(classToValidate);
         void *classChainIdentifyingLoaderForClazzToValidate = sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazzToValidate);
         uintptrj_t classChainOffsetInSharedCacheForCL = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazzToValidate);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCacheForCL;
         cursor += SIZEPOINTER;

         //store the classchain's offset for the class that needs to be validated in the second run
         void *romClass = (void *)fej9->getPersistentClassPointerFromClassPointer(classToValidate);
         uintptrj_t *classChainForClassToValidate = (uintptrj_t *) aotCI->_classChain;
         void* classChainOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(classChainForClassToValidate);
         *(uintptrj_t *)cursor = (uintptrj_t) classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_HCR:
         {
         *(uintptrj_t*)cursor = (uintptrj_t)relocation->getTargetAddress();
         cursor += SIZEPOINTER;
         }
         break;

      case TR_DebugCounter:
         {
         TR::DebugCounterBase *counter = (TR::DebugCounterBase *) relocation->getTargetAddress();
         if (!counter || !counter->getReloData() || !counter->getName())
            comp->failCompilation<TR::CompilationException>("Failed to generate debug counter relo data");

         TR::DebugCounterReloData *counterReloData = counter->getReloData();

         uintptrj_t offset = (uintptrj_t)fej9->sharedCache()->rememberDebugCounterName(counter->getName());

         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_callerIndex;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_bytecodeIndex;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = offset;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_delta;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_fidelity;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_staticDelta;
         cursor += SIZEPOINTER;
         }
         break;

      default:
      	return cursor;
      }
   return cursor;
   }

uint32_t J9::X86::AheadOfTimeCompile::_relocationTargetTypeToHeaderSizeMap[TR_NumExternalRelocationKinds] =
   {
// FIXME this code needs to be cleaned up by having here access to the platform pointer size
//       or by defining in the runtime.hpp the sizes of the relocation items
#if defined (TR_HOST_64BIT)
   24,                                              // TR_ConstantPool                        = 0
   8,                                               // TR_HelperAddress                       = 1
   24,                                              // TR_RelativeMethodAddress               = 2
   8,                                               // TR_AbsoluteMethodAddress               = 3
   40,                                              // TR_DataAddress                         = 4
   24,                                              // TR_ClassObject                         = 5
   24,                                              // TR_MethodObject                        = 6
   24,                                              // TR_InterfaceObject                     = 7
   8,                                               // TR_AbsoluteHelperAddress               = 8
   16,                                              // TR_FixedSeqAddress                     = 9
   16,                                              // TR_FixedSeq2Address                    = 10
   32,                                              // TR_JNIVirtualTargetAddress	      = 11
   32,                                              // TR_JNIStaticTargetAddress              = 12
   4,                                               // Dummy for TR_ArrayCopyHelper           = 13
   4,                                               // Dummy for TR_ArrayCopyToc              = 14
   8,                                               // TR_BodyInfoAddress                     = 15
   24,                                              // TR_Thunks                              = 16
   32,                                              // TR_StaticRamMethodConst                = 17
   24,                                              // TR_Trampolines                         = 18
   8,                                               // TR_PicTrampolines                      = 19
   16,                                              // TR_CheckMethodEnter                    = 20
   8,                                               // TR_RamMethod                           = 21
   16,                                              // TR_RamMethodSequence                   = 22
   16,                                              // TR_RamMethodSequenceReg                = 23
   48,                                              // TR_VerifyClassObjectForAlloc           = 24
   24,                                              // TR_ConstantPoolOrderedPair             = 25
   8,                                               // TR_AbsoluteMethodAddressOrderedPair    = 26
   40,                                              // TR_VerifyRefArrayForAlloc              = 27
   24,                                              // TR_J2IThunks                           = 28
   16,                                              // TR_GlobalValue                         = 29
   4,                                               // dummy for TR_BodyInfoAddress           = 30
   40,                                              // TR_ValidateInstanceField               = 31
   48,                                              // TR_InlinedStaticMethodWithNopGuard     = 32
   48,                                              // TR_InlinedSpecialMethodWithNopGuard    = 33
   48,                                              // TR_InlinedVirtualMethodWithNopGuard    = 34
   48,                                              // TR_InlinedInterfaceMethodWithNopGuard  = 35
   32,                                              // TR_SpecialRamMethodConst               = 36
   48,                                              // TR_InlinedHCRMethod                    = 37
   40,                                              // TR_ValidateStaticField                 = 38
   40,                                              // TR_ValidateClass                       = 39
   32,                                              // TR_ClassAddress                        = 40
   16,                                              // TR_HCR                                 = 41
   64,                                              // TR_ProfiledMethodGuardRelocation       = 42
   64,                                              // TR_ProfiledClassGuardRelocation        = 43
   0,                                               // TR_HierarchyGuardRelocation            = 44
   0,                                               // TR_AbstractGuardRelocation             = 45
   64,                                              // TR_ProfiledInlinedMethodRelocation     = 46
   40,                                              // TR_MethodPointer                       = 47
   32,                                              // TR_ClassPointer                        = 48
   16,                                              // TR_CheckMethodExit                     = 49
   24,                                              // TR_ValidateArbitraryClass              = 50
   0,                                               // TR_EmitClass(not used)                 = 51
   32,                                              // TR_JNISpecialTargetAddress             = 52
   32,                                              // TR_VirtualRamMethodConst               = 53
   40,                                              // TR_InlinedInterfaceMethod              = 54
   40,                                              // TR_InlinedVirtualMethod                = 55
   0,                                               // TR_NativeMethodAbsolute                = 56,
   0,                                               // TR_NativeMethodRelative                = 57,
   32,                                              // TR_ArbitraryClassAddress               = 58,
   56,                                              // TR_DebugCounter                        = 59
#else

   12,                                              // TR_ConstantPool                        = 0
   8,                                               // TR_HelperAddress                       = 1
   12,                                              // TR_RelativeMethodAddress               = 2
   4,                                               // TR_AbsoluteMethodAddress               = 3
   20,                                              // TR_DataAddress                         = 4
   12,                                              // TR_ClassObject                         = 5
   12,                                              // TR_MethodObject                        = 6
   12,                                              // TR_InterfaceObject                     = 7
   8,                                               // TR_AbsoluteHelperAddress               = 8
   8,                                               // TR_FixedSeqAddress                     = 9
   8,                                               // TR_FixedSeq2Address                    = 10
   16,                                              // TR_JNIVirtualTargetAddress             = 11
   16,                                              // TR_JNIStaticTargetAddress              = 12
   4,                                               // Dummy for TR_ArrayCopyHelper           = 13
   4,                                               // Dummy for TR_ArrayCopyToc              = 14
   4,                                               // TR_BodyInfoAddress                     = 15
   12,                                              // TR_Thunks                              = 16
   16,                                              // TR_StaticRamMethodConst                = 17
   12,                                              // TR_Trampolines                         = 18
   8,                                               // TR_PicTrampolines                      = 19
   8,                                               // TR_CheckMethodEnter                    = 20
   4,                                               // TR_RamMethod                           = 21
   8,                                               // TR_RamMethodSequence                   = 22
   8,                                               // TR_RamMethodSequenceReg                = 23
   24,                                              // TR_VerifyClassObjectForAlloc           = 24
   12,                                              // TR_ConstantPoolOrderedPair             = 25
   8,                                               // TR_AbsoluteMethodAddressOrderedPair    = 26
   20,                                              // TR_VerifyRefArrayForAlloc              = 27
   12,                                              // TR_J2IThunks                           = 28
   8,                                               // TR_GlobalValue                         = 29
   4,                                               // TR_BodyInfoAddressLoad                 = 30
   20,                                              // TR_ValidateInstanceField               = 31
   24,                                              // TR_InlinedStaticMethodWithNopGuard     = 32
   24,                                              // TR_InlinedSpecialMethodWithNopGuard    = 33
   24,                                              // TR_InlinedVirtualMethodWithNopGuard    = 34
   24,                                              // TR_InlinedInterfaceMethodWithNopGuard  = 35
   16,                                              // TR_SpecialRamMethodConst               = 36
   24,                                              // TR_InlinedHCRMethod                    = 37
   20,                                              // TR_ValidateStaticField                 = 38
   20,                                              // TR_ValidateClass                       = 39
   16,                                              // TR_ClassAddress                        = 40
   8,                                               // TR_HCR                                 = 41
   32,                                              // TR_ProfiledMethodGuardRelocation       = 42
   32,                                              // TR_ProfiledClassGuardRelocation        = 43
   0,                                               // TR_HierarchyGuardRelocation            = 44
   0,                                               // TR_AbstractGuardRelocation             = 45
   32,                                              // TR_ProfiledInlinedMethodRelocation     = 46
   20,                                              // TR_MethodPointer                       = 47
   16,                                              // TR_ClassPointer                        = 48
   8,                                               // TR_CheckMethodExit                     = 49
   12,                                              // TR_ValidateArbitraryClass              = 50
   0,                                               // TR_EmitClass(not used)                 = 51
   16,                                              // TR_JNISpecialTargetAddress             = 52
   16,                                              // TR_VirtualRamMethodConst               = 53
   20,                                              // TR_InlinedInterfaceMethod              = 54
   20,                                              // TR_InlinedVirtualMethod                = 55
   0,                                               // TR_NativeMethodAbsolute                = 56,
   0,                                               // TR_NativeMethodRelative                = 57,
   16,                                              // TR_ArbitraryClassAddress               = 58,
   28                                               // TR_DebugCounter                        = 59
#endif
   };

