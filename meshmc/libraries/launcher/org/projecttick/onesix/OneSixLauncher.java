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

package org.projecttick.onesix;

import org.projecttick.*;

import java.applet.Applet;
import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public class OneSixLauncher implements MeshMC
{
    // parameters, separated from ParamBucket
    private List<String> libraries;
    private List<String> mcparams;
    private List<String> mods;
    private List<String> jarmods;
    private List<String> coremods;
    private List<String> traits;
    private String appletClass;
    private String mainClass;
    private String nativePath;
    private String userName, sessionId;
    private String windowTitle;
    private String windowParams;

    // secondary parameters
    private int winSizeW;
    private int winSizeH;
    private boolean maximize;
    private String cwd;

    private String serverAddress;
    private String serverPort;

    // the much abused system classloader, for convenience (for further abuse)
    private ClassLoader cl;

    private void processParams(ParamBucket params) throws NotFoundException
    {
        libraries = params.all("cp");
        mcparams = params.allSafe("param", new ArrayList<String>() );
        mainClass = params.firstSafe("mainClass", "net.minecraft.client.Minecraft");
        appletClass = params.firstSafe("appletClass", "net.minecraft.client.MinecraftApplet");
        traits = params.allSafe("traits", new ArrayList<String>());
        nativePath = params.first("natives");

        userName = params.first("userName");
        sessionId = params.first("sessionId");
        windowTitle = params.firstSafe("windowTitle", "Minecraft");
        windowParams = params.firstSafe("windowParams", "854x480");

        serverAddress = params.firstSafe("serverAddress", null);
        serverPort = params.firstSafe("serverPort", null);

        cwd = System.getProperty("user.dir");

        winSizeW = 854;
        winSizeH = 480;
        maximize = false;

        String[] dimStrings = windowParams.split("x");

        if (windowParams.equalsIgnoreCase("max"))
        {
            maximize = true;
        }
        else if (dimStrings.length == 2)
        {
            try
            {
                winSizeW = Integer.parseInt(dimStrings[0]);
                winSizeH = Integer.parseInt(dimStrings[1]);
            } catch (NumberFormatException ignored) {}
        }
    }

    int legacyLaunch()
    {
        // Get the Minecraft Class and set the base folder
        Class<?> mc;
        try
        {
            mc = cl.loadClass(mainClass);

            Field f = Utils.getMCPathField(mc);

            if (f == null)
            {
                System.err.println("Could not find Minecraft path field.");
            }
            else
            {
                f.setAccessible(true);
                f.set(null, new File(cwd));
            }
        } catch (Exception e)
        {
            System.err.println("Could not set base folder. Failed to find/access Minecraft main class:");
            e.printStackTrace(System.err);
            return -1;
        }

        System.setProperty("minecraft.applet.TargetDirectory", cwd);

        if(!traits.contains("noapplet"))
        {
            Utils.log("Launching with applet wrapper...");
            try
            {
                Class<?> mcAppletClass = cl.loadClass(appletClass);
                Applet mcappl = Applet.class.cast(
                    mcAppletClass.getDeclaredConstructor().newInstance()
                );
                LegacyFrame mcWindow = new LegacyFrame(windowTitle);
                mcWindow.start(mcappl, userName, sessionId, winSizeW, winSizeH, maximize, serverAddress, serverPort);
                return 0;
            } catch (Exception e)
            {
                Utils.log("Applet wrapper failed:", "Error");
                e.printStackTrace(System.err);
                Utils.log();
                Utils.log("Falling back to using main class.");
            }
        }

        // init params for the main method to chomp on.
        String[] paramsArray = mcparams.toArray(new String[mcparams.size()]);
        try
        {
            mc.getMethod("main", String[].class).invoke(null, (Object) paramsArray);
            return 0;
        } catch (Exception e)
        {
            Utils.log("Failed to invoke the Minecraft main class:", "Fatal");
            e.printStackTrace(System.err);
            return -1;
        }
    }

    int launchWithMainClass()
    {
        // window size, title and state, onesix
        if (maximize)
        {
            // FIXME: there is no good way to maximize the minecraft window in onesix.
            // the following often breaks linux screen setups
            // mcparams.add("--fullscreen");
        }
        else
        {
            mcparams.add("--width");
            mcparams.add(Integer.toString(winSizeW));
            mcparams.add("--height");
            mcparams.add(Integer.toString(winSizeH));
        }

        if (serverAddress != null)
        {
            mcparams.add("--server");
            mcparams.add(serverAddress);
            mcparams.add("--port");
            mcparams.add(serverPort);
        }

        // Get the Minecraft Class.
        Class<?> mc;
        try
        {
            mc = cl.loadClass(mainClass);
        } catch (ClassNotFoundException e)
        {
            System.err.println("Failed to find Minecraft main class:");
            e.printStackTrace(System.err);
            return -1;
        }

        // get the main method.
        Method meth;
        try
        {
            meth = mc.getMethod("main", String[].class);
        } catch (NoSuchMethodException e)
        {
            System.err.println("Failed to acquire the main method:");
            e.printStackTrace(System.err);
            return -1;
        }

        // init params for the main method to chomp on.
        String[] paramsArray = mcparams.toArray(new String[mcparams.size()]);
        try
        {
            // static method doesn't have an instance
            meth.invoke(null, (Object) paramsArray);
        } catch (Exception e)
        {
            System.err.println("Failed to start Minecraft:");
            e.printStackTrace(System.err);
            return -1;
        }
        return 0;
    }

    @Override
    public int launch(ParamBucket params)
    {
        // get and process the launch script params
        try
        {
            processParams(params);
        } catch (NotFoundException e)
        {
            System.err.println("Not enough arguments.");
            e.printStackTrace(System.err);
            return -1;
        }

        // grab the system classloader and ...
        cl = ClassLoader.getSystemClassLoader();

        if (traits.contains("legacyLaunch") || traits.contains("alphaLaunch") )
        {
            // legacy launch uses the applet wrapper
            return legacyLaunch();
        }
        else
        {
            // normal launch just calls main()
            return launchWithMainClass();
        }
    }
}
