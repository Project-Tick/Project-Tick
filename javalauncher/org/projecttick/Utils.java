/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   MeshMC - A Custom Launcher for Minecraft
 *   Copyright (C) 2026 Project Tick
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   Linking this library statically or dynamically with other modules is
 *   making a combined work based on this library. Thus, the terms and
 *   conditions of the GNU General Public License cover the whole
 *   combination.
 *
 *   As a special exception, the copyright holders of this library give
 *   you permission to link this library with independent modules to
 *   produce an executable, regardless of the license terms of these
 *   independent modules, and to copy and distribute the resulting
 *   executable under terms of your choice, provided that you also meet,
 *   for each linked independent module, the terms and conditions of the
 *   license of that module. An independent module is a module which is
 *   not derived from or based on this library. If you modify this
 *   library, you may extend this exception to your version of the
 *   library, but you are not obliged to do so. If you do not wish to do
 *   so, delete this exception statement from your version.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *  This file incorporates work covered by the following copyright and
 *  permission notice:
 *
 * Copyright 2012-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.projecttick;

import java.io.*;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.*;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class Utils
{
    /**
     * Combine two parts of a path.
     *
     * @param path1
     * @param path2
     * @return the paths, combined
     */
    public static String combine(String path1, String path2)
    {
        File file1 = new File(path1);
        File file2 = new File(file1, path2);
        return file2.getPath();
    }

    /**
     * Join a list of strings into a string using a separator!
     *
     * @param strings   the string list to join
     * @param separator the glue
     * @return the result.
     */
    public static String join(List<String> strings, String separator)
    {
        StringBuilder sb = new StringBuilder();
        String sep = "";
        for (String s : strings)
        {
            sb.append(sep).append(s);
            sep = separator;
        }
        return sb.toString();
    }

    /**
     * Finds a field that looks like a Minecraft base folder in a supplied class
     *
     * @param mc the class to scan
     */
    public static Field getMCPathField(Class<?> mc)
    {
        Field[] fields = mc.getDeclaredFields();

        for (Field f : fields)
        {
            if (f.getType() != File.class)
            {
                // Has to be File
                continue;
            }
            if (f.getModifiers() != (Modifier.PRIVATE + Modifier.STATIC))
            {
                // And Private Static.
                continue;
            }
            return f;
        }
        return null;
    }

    /**
     * Log to MeshMC console
     *
     * @param message A String containing the message
     * @param level   A String containing the level name. See MinecraftLauncher::getLevel()
     */
    public static void log(String message, String level)
    {
        // Kinda dirty
        String tag = "!![" + level + "]!";
        System.out.println(tag + message.replace("\n", "\n" + tag));
    }

    public static void log(String message)
    {
        log(message, "MeshMC");
    }

    public static void log()
    {
        System.out.println();
    }
}

