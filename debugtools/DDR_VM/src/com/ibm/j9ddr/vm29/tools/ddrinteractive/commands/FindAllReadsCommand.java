/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.util.PatternString;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.HashTable;
import com.ibm.j9ddr.vm29.j9.ModuleHashTable;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.JavaVersionHelper;

/**
 * FindAllReads command displays all modules that read the target module
 * 
 * Example:
 * 			!findallreads 0x00000130550DBB58
 * Example output: 
 * 			java.xml			!j9module 0x000001305F46BCB8
 * 			java.base			!j9module 0x00000130550C7E88
 *			Found 2 module(s) that read(s) from !j9module	0x00000130550DBB58
 */
public class FindAllReadsCommand extends Command{

	public FindAllReadsCommand()
	{
		addCommand("findallreads", "<targetModuleAddress>", "find all modules that read target module");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		if (args.length != 1) {
			CommandUtils.dbgPrint(out, "Usage: !findallreads <targetModuleAddress>\n");
			return;
		}
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (JavaVersionHelper.ensureJava9AndUp(vm, out)) {
				J9PoolPointer pool = vm.modularityPool();
				Pool<J9ModulePointer> modulePool = Pool.fromJ9Pool(pool, J9ModulePointer.class);
				SlotIterator<J9ModulePointer> poolIterator = modulePool.iterator();
				J9ModulePointer modulePtr = null;

				int hitCount = 0;
				String searchModuleAddress = args[0];
				PatternString pattern = new PatternString(searchModuleAddress);

				while (poolIterator.hasNext()) {
					modulePtr = poolIterator.next();
					/*
					 * Since both packages and modules could be in the pool, the
					 * iterator checks on the region and make sure that it is
					 * pointing to a module. (For modules, the first field must
					 * be in heap since it is an object while for j9package it
					 * is a J9UTF8)
					 */
					if (ObjectModel.isPointerInHeap(vm, modulePtr.moduleName())) {
						String moduleName = J9ObjectHelper.stringValue(modulePtr.moduleName());
						String hexAddress = modulePtr.getHexAddress();
						J9HashTablePointer readTable = modulePtr.readAccessHashTable();
						HashTable<J9ModulePointer> moduleHashTable = ModuleHashTable.fromJ9HashTable(readTable);
						SlotIterator<J9ModulePointer> slotIterator = moduleHashTable.iterator();
						J9ModulePointer readModulePtr = null;
						while (slotIterator.hasNext()) {
							readModulePtr = slotIterator.next();
							String readHexAddress = readModulePtr.getHexAddress();
							if (pattern.isMatch(readHexAddress)) {
								hitCount++;
								out.printf("%-30s !j9module %s%n", moduleName, hexAddress);
								break;
							}
						}
					}
				}
				out.println(String.format("Found %d module(s) that read(s) from !j9module %s%n", hitCount,
						searchModuleAddress));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}

