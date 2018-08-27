/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.valhalla;

import java.lang.reflect.*;
import org.objectweb.asm.Attribute;

public class NestTest {
	public static void main(String[] args) throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		try {
			byte[] aBytes = ClassGenerator.generateClass("test/A",
					new Attribute[] { new NestMembersAttribute(new String[] { "test/B", "test/C", "test/D" }) });
			Class<?> aClazz = classloader.getClass("test.A", aBytes);
			byte[] bBytes = ClassGenerator.generateClass("test/B", new Attribute[] { new NestHostAttribute("test/A") });
			Class<?> bClazz = classloader.getClass("test.B", bBytes);
			byte[] cBytes = ClassGenerator.generateClass("test/C", new Attribute[] { new NestHostAttribute("test/A") });
			Class<?> cClazz = classloader.getClass("test.C", cBytes);
			byte[] dBytes = ClassGenerator.generateClass("test/D", new Attribute[] { new NestHostAttribute("test/A") });
			Class<?> dClazz = classloader.getClass("test.D", dBytes);
			byte[] eBytes = ClassGenerator.generateClass("test/E", new Attribute[] { new NestMembersAttribute(new String[] {})});
			Class<?> eClazz = classloader.getClass("test.E", eBytes);
			byte[] fBytes = ClassGenerator.generateClass("test/F", new Attribute[] { new NestHostAttribute("test/F") });
			Class<?> fClazz = classloader.getClass("test.F", fBytes);
			byte[] gBytes = ClassGenerator.generateClass("test/G", new Attribute[] { new NestHostAttribute("test/G"), new NestMembersAttribute(new String[] {}) });
			Class<?> gClazz = classloader.getClass("test.G", gBytes);	
			Object aInstance = aClazz.getDeclaredConstructor().newInstance();
			Object bInstance = bClazz.getDeclaredConstructor().newInstance();
			Object cInstance = cClazz.getDeclaredConstructor().newInstance();
			Object dInstance = dClazz.getDeclaredConstructor().newInstance();
			Object eInstance = eClazz.getDeclaredConstructor().newInstance();
			Object fInstance = fClazz.getDeclaredConstructor().newInstance();
			Object gInstance = gClazz.getDeclaredConstructor().newInstance();
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}

	}
}