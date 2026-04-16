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

package org.projecttick.modern;

import org.projecttick.MeshMC;
import org.projecttick.ParamBucket;
import org.projecttick.Utils;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Modern launcher implementation for Minecraft 1.0 and above.
 *
 * Unlike OneSixLauncher which loads the game in-process via reflection,
 * ModernLauncher spawns Minecraft as a separate child process using the
 * Java binary specified by MeshMC. This decouples the JVM version
 * running MeshMC library from the JVM version required by the game,
 * allowing Minecraft versions that require Java 21+ or newer (e.g., class
 * file version 69.0 / Java 25) to be launched correctly even when the
 * launcher itself runs on an older JVM.
 *
 * Parameters consumed from ParamBucket:
 *   javaPath     - Path to the Java binary for the game process (required)
 *   cp           - Classpath entries, one per entry (required)
 *   mainClass    - Main class to invoke (default: net.minecraft.client.main.Main)
 *   param        - Game arguments, one per entry
 *   jvmArg       - JVM arguments to forward to the game process, one per arg
 *   natives      - Path to the native libraries directory
 *   serverAddress - Server address for direct-connect on launch (optional)
 *   serverPort    - Server port for direct-connect on launch (optional)
 */
public class ModernLauncher implements MeshMC
{
    private static boolean isProcessAlive(Process process)
    {
        try
        {
            process.exitValue();
            return false;
        }
        catch (IllegalThreadStateException ignored)
        {
            return true;
        }
    }

    @Override
    public int launch(ParamBucket params)
    {
        try
        {
            return doLaunch(params);
        }
        catch (Exception e)
        {
            Utils.log("ModernLauncher encountered a fatal error: " + e.getMessage(), "Error");
            e.printStackTrace();
            return 1;
        }
    }

    private int doLaunch(ParamBucket params) throws Exception
    {
        // --- Java binary ---
        String javaPath = params.firstSafe("javaPath", "java");
        Utils.log("Java binary: " + javaPath);

        // --- Main class ---
        String mainClass = params.firstSafe("mainClass", "net.minecraft.client.main.Main");
        Utils.log("Main class: " + mainClass);

        // --- Classpath ---
        // "cp"  = regular game libraries
        // "ext" = native-classifier JARs (e.g. lwjgl-3.x-natives-linux.jar) that
        //         LWJGL 3's SharedLibraryLoader needs to find on the classpath.
        //         OneSixLauncher works without these because it runs in-process and
        //         MeshMC JVM already has them on its own classpath.  For
        //         ModernLauncher we must include them explicitly.
        List<String> cpEntries  = params.allSafe("cp",  Collections.<String>emptyList());
        List<String> extEntries = params.allSafe("ext", Collections.<String>emptyList());
        List<String> allCpEntries = new ArrayList<String>(cpEntries);
        allCpEntries.addAll(extEntries);
        if (allCpEntries.isEmpty())
        {
            Utils.log("No classpath entries provided to ModernLauncher.", "Error");
            return 1;
        }
        String classpath = buildClassPath(allCpEntries);

        // --- Native library path ---
        String natives = params.firstSafe("natives", "");

        // --- JVM arguments (Xmx, Xms, GC flags, platform-specific flags, etc.) ---
        List<String> jvmArgs = params.allSafe("jvmArg", Collections.<String>emptyList());

        // --- Game arguments ---
        // param entries are already pre-expanded by the C++ launcher (auth tokens,
        // game directory, asset index, resolution, etc.)
        List<String> gameArgs = new ArrayList<String>(
            params.allSafe("param", Collections.<String>emptyList())
        );

        // Direct-connect: server address / port are passed as separate keys and
        // must be appended to game args manually (processMinecraftArgs skips them
        // when a launch script is used).
        String serverAddress = params.firstSafe("serverAddress", "");
        String serverPort    = params.firstSafe("serverPort", "");
        if (serverAddress != null && !serverAddress.isEmpty())
        {
            gameArgs.add("--server");
            gameArgs.add(serverAddress);
            if (serverPort != null && !serverPort.isEmpty())
            {
                gameArgs.add("--port");
                gameArgs.add(serverPort);
            }
        }

        // --- Build the full command ---
        List<String> command = buildCommand(javaPath, jvmArgs, natives, classpath, mainClass, gameArgs);

        // Log the full command for diagnostics (visible in launcher console)
        StringBuilder cmdLog = new StringBuilder("Spawning game process:\n  CMD: ");
        for (String s : command)
        {
            cmdLog.append(s).append(" ");
        }
        Utils.log(cmdLog.toString().trim());

        ProcessBuilder pb = new ProcessBuilder(command);
        // Let the game process inherit the working directory from us (set by LauncherPartLaunch)
        pb.directory(null);

        // Set JAVA_HOME so the game and any native launchers can locate Java correctly
        File javaFile = new File(javaPath);
        File javaParent = javaFile.getParentFile();
        if (javaParent != null && javaParent.getParentFile() != null)
        {
            pb.environment().put("JAVA_HOME", javaParent.getParent());
        }

        final Process process = pb.start();

        // Pipe stdout and stderr from the child process to our own streams so
        // the C++ launcher's LoggedProcess can capture and display them.
        Thread outPipe = pipeStream(process.getInputStream(), System.out, "stdout");
        Thread errPipe = pipeStream(process.getErrorStream(), System.err, "stderr");
        outPipe.start();
        errPipe.start();

        // Shutdown hook: ensure the game process is terminated if MeshMC is
        // forcefully killed (e.g., SIGKILL from within MeshMC UI).
        Runtime.getRuntime().addShutdownHook(new Thread(new Runnable()
        {
            @Override
            public void run()
            {
                if (isProcessAlive(process))
                {
                    process.destroy();
                }
            }
        }, "ModernLauncher-ShutdownHook"));

        // Block until the game process exits
        int exitCode = process.waitFor();

        outPipe.join();
        errPipe.join();

        if (exitCode != 0)
        {
            Utils.log("Game process exited with code " + exitCode, "Error");
        }

        return exitCode;
    }

    private String buildClassPath(List<String> entries)
    {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < entries.size(); i++)
        {
            if (i > 0)
                sb.append(File.pathSeparator);
            sb.append(entries.get(i));
        }
        return sb.toString();
    }

    private List<String> buildCommand(
        String javaPath,
        List<String> jvmArgs,
        String natives,
        String classpath,
        String mainClass,
        List<String> gameArgs)
    {
        List<String> cmd = new ArrayList<String>();

        // Java executable
        cmd.add(javaPath);

        // JVM arguments (memory settings, GC options, platform flags, etc.)
        cmd.addAll(jvmArgs);

        // Native library path - needed for LWJGL and other native dependencies.
        // Pass both the standard JVM property and the LWJGL-specific property:
        //   java.library.path   - used by java.lang.System.loadLibrary()
        //   org.lwjgl.librarypath - checked first by LWJGL 3.3+ before java.library.path
        if (natives != null && !natives.isEmpty())
        {
            cmd.add("-Djava.library.path=" + natives);
            cmd.add("-Dorg.lwjgl.librarypath=" + natives);
        }

        // Classpath
        if (!classpath.isEmpty())
        {
            cmd.add("-cp");
            cmd.add(classpath);
        }

        // Main class
        cmd.add(mainClass);

        // Game arguments
        cmd.addAll(gameArgs);

        return cmd;
    }

    /**
     * Reads bytes from {@code src} and writes them to {@code dst} on a dedicated
     * daemon thread so neither stream blocks the main thread.
     */
    private Thread pipeStream(final InputStream src, final PrintStream dst, final String name)
    {
        Thread t = new Thread(new Runnable()
        {
            @Override
            public void run()
            {
                byte[] buffer = new byte[8192];
                int read;
                try
                {
                    while ((read = src.read(buffer)) != -1)
                    {
                        dst.write(buffer, 0, read);
                        dst.flush();
                    }
                }
                catch (IOException e)
                {
                    // Stream closed - expected when the child process exits
                }
            }
        }, "ModernLauncher-" + name + "-pipe");
        t.setDaemon(true);
        return t;
    }
}
