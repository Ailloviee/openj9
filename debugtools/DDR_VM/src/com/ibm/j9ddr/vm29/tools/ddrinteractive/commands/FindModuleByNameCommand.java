package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.util.PatternString;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

public class FindModuleByNameCommand extends Command
{
	public FindModuleByNameCommand()
	{
		addCommand("findmodulebyname","<moduleName>","find the modules corresponding to its module name");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		if (args.length == 0) {
			printUsage(out);
			return;
		}
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9PoolPointer pool = vm.modularityPool();
			Pool<J9ModulePointer> modulePool = Pool.fromJ9Pool(pool, J9ModulePointer.class);
			SlotIterator<J9ModulePointer> poolIterator = modulePool.iterator();
			J9ModulePointer modulePtr = null;
			
			int hitCount = 0;
			String searchModuleName = args[0];
			PatternString pattern = new PatternString (searchModuleName);
	
			out.println(String.format(
					"Searching for modules named '%1$s' in VM=%2$s",
					searchModuleName, Long.toHexString(vm.getAddress())));
			while (poolIterator.hasNext()) {
				modulePtr = poolIterator.next();
				if (ObjectModel.isPointerInHeap(vm, modulePtr.moduleName()) && pattern.isMatch(J9ObjectHelper.stringValue(modulePtr.moduleName()))) {
					hitCount++;
					
					String moduleName = J9ObjectHelper.stringValue(modulePtr.moduleName());
					String hexAddress = modulePtr.getHexAddress();
					out.printf("%s			!j9module %s\n", moduleName, hexAddress);
				}
			}	
			out.println(String.format("Found %1$d module(s) named %2$s",
					hitCount, searchModuleName));
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	 /**
     * Prints the usage for the findmodulebyname command.
     *
     * @param out  the PrintStream the usage statement prints to
     */
	private void printUsage (PrintStream out) {
		out.println("findmodulebyname <moduleName> - find the module corresponding to its module name");
	}
}
