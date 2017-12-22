/*******************************************************************************
 * Copyright (c) 2013, 2017 IBM Corp. and others
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
package j9vm.test.classloader;

/**
 * ClassLoader used to testing lazy classloader init (see Design 61796).
 *
 * @author braddill@ca.ibm.com Created 1 February 2013.
 */
public class EmptyClassLoader extends ClassLoader {

	/*
	 * Java 1.6 Bytecode for class:
	 *
	 * package j9vm.test.classloader;
	 *
	 * public class Dummy {}
	 */
	public static final int[]	class_Dummy	= { 0xca, 0xfe, 0xba, 0xbe, 0x0, 0x0, 0x0, 0x32, 0x0, 0x10, 0x7, 0x0, 0x2, 0x1, 0x0,
		0x1b, 0x6a, 0x39, 0x76, 0x6d, 0x2f, 0x74, 0x65, 0x73, 0x74, 0x2f, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x6c, 0x6f, 0x61,
		0x64, 0x65, 0x72, 0x2f, 0x44, 0x75, 0x6d, 0x6d, 0x79, 0x7, 0x0, 0x4, 0x1, 0x0, 0x10, 0x6a, 0x61, 0x76, 0x61, 0x2f,
		0x6c, 0x61, 0x6e, 0x67, 0x2f, 0x4f, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x1, 0x0, 0x6, 0x3c, 0x69, 0x6e, 0x69, 0x74, 0x3e,
		0x1, 0x0, 0x3, 0x28, 0x29, 0x56, 0x1, 0x0, 0x4, 0x43, 0x6f, 0x64, 0x65, 0xa, 0x0, 0x3, 0x0, 0x9, 0xc, 0x0, 0x5, 0x0,
		0x6, 0x1, 0x0, 0xf, 0x4c, 0x69, 0x6e, 0x65, 0x4e, 0x75, 0x6d, 0x62, 0x65, 0x72, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x1,
		0x0, 0x12, 0x4c, 0x6f, 0x63, 0x61, 0x6c, 0x56, 0x61, 0x72, 0x69, 0x61, 0x62, 0x6c, 0x65, 0x54, 0x61, 0x62, 0x6c,
		0x65, 0x1, 0x0, 0x4, 0x74, 0x68, 0x69, 0x73, 0x1, 0x0, 0x1d, 0x4c, 0x6a, 0x39, 0x76, 0x6d, 0x2f, 0x74, 0x65, 0x73,
		0x74, 0x2f, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x6c, 0x6f, 0x61, 0x64, 0x65, 0x72, 0x2f, 0x44, 0x75, 0x6d, 0x6d, 0x79,
		0x3b, 0x1, 0x0, 0xa, 0x53, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x46, 0x69, 0x6c, 0x65, 0x1, 0x0, 0xa, 0x44, 0x75, 0x6d,
		0x6d, 0x79, 0x2e, 0x6a, 0x61, 0x76, 0x61, 0x0, 0x21, 0x0, 0x1, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x1, 0x0,
		0x5, 0x0, 0x6, 0x0, 0x1, 0x0, 0x7, 0x0, 0x0, 0x0, 0x2f, 0x0, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0x5, 0x2a, 0xb7, 0x0, 0x8,
		0xb1, 0x0, 0x0, 0x0, 0x2, 0x0, 0xa, 0x0, 0x0, 0x0, 0x6, 0x0, 0x1, 0x0, 0x0, 0x0, 0x12, 0x0, 0xb, 0x0, 0x0, 0x0, 0xc,
		0x0, 0x1, 0x0, 0x0, 0x0, 0x5, 0x0, 0xc, 0x0, 0xd, 0x0, 0x0, 0x0, 0x1, 0x0, 0xe, 0x0, 0x0, 0x0, 0x2, 0x0, 0xf };

	/**
	 * Load class j9vm.test.classloader.Dummy from above bytecode, otherwise use
	 * parent classloader.
	 *
	 * @param name
	 *            the fully qualified classname to load (using . to separate
	 *            packages).
	 *
	 * @return the loaded class.
	 */
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public synchronized Class loadClass(String name) {

		Class clazz = null;

		if (name.equals("j9vm.test.classloader.Dummy")) {
			clazz = loadClass();
		} else {
			try {
				clazz = super.loadClass(name);
			} catch (ClassNotFoundException e) {
				e.printStackTrace();
			}
		}
		return clazz;
	}

	/**
	 * Load j9vm.test.classloader.Dummy from above bytecode.
	 *
	 * @return Class j9vm.test.classloader.Dummy
	 */
	@SuppressWarnings("rawtypes")
	public synchronized Class loadClass() {

		byte[] bytes = getDummyBytecode();

		return defineClass("j9vm.test.classloader.Dummy", bytes, 0,
				bytes.length);
	}

	/**
	 * Get the bytecode for the j9vm.test.classloader.Dummy class.
	 *
	 * @return the bytecode for the j9vm.test.classloader.Dummy class.
	 */
	public static byte[] getDummyBytecode() {
		byte[] bytes = new byte[class_Dummy.length];

		for (int i = 0; i < class_Dummy.length; i++)
			bytes[i] = (byte) class_Dummy[i];
		return bytes;
	}
}
