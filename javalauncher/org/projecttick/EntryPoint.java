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

import org.projecttick.modern.ModernLauncher;
import org.projecttick.onesix.OneSixLauncher;

import java.io.*;
import java.nio.charset.Charset;

public class EntryPoint
{
    private enum Action
    {
        Proceed,
        Launch,
        Abort
    }

    public static void main(String[] args)
    {
        EntryPoint listener = new EntryPoint();
        int retCode = listener.listen();
        if (retCode != 0)
        {
            System.out.println("Exiting with " + retCode);
            System.exit(retCode);
        }
    }

    private Action parseLine(String inData) throws ParseException
    {
        String[] pair = inData.split(" ", 2);

        if(pair.length == 1)
        {
            String command = pair[0];
            if (pair[0].equals("launch"))
                return Action.Launch;

            else if (pair[0].equals("abort"))
                return Action.Abort;

            else throw new ParseException("Error while parsing:" + pair[0]);
        }

        if(pair.length != 2)
            throw new ParseException("Pair length is not 2.");

        String command = pair[0];
        String param = pair[1];

        if(command.equals("launcher"))
        {
            if(param.equals("onesix"))
            {
                m_launcher = new OneSixLauncher();
                Utils.log("Using onesix launcher.");
                Utils.log();
                return Action.Proceed;
            }
            else if(param.equals("modern"))
            {
                m_launcher = new ModernLauncher();
                Utils.log("Using modern launcher (subprocess mode).");
                Utils.log();
                return Action.Proceed;
            }
            else
                throw new ParseException("Invalid launcher type: " + param);
        }

        m_params.add(command, param);
        //System.out.println(command + " : " + param);
        return Action.Proceed;
    }

    public int listen()
    {
        BufferedReader buffer;
        try
        {
            buffer = new BufferedReader(new InputStreamReader(System.in, "UTF-8"));
        } catch (UnsupportedEncodingException e)
        {
            System.err.println("For some reason, your java does not support UTF-8. Consider living in the current century.");
            e.printStackTrace();
            return 1;
        }
        boolean isListening = true;
        boolean isAborted = false;
        // Main loop
        while (isListening)
        {
            String inData;
            try
            {
                // Read from the pipe one line at a time
                inData = buffer.readLine();
                if (inData != null)
                {
                    Action a = parseLine(inData);
                    if(a == Action.Abort)
                    {
                        isListening = false;
                        isAborted = true;
                    }
                    if(a == Action.Launch)
                    {
                        isListening = false;
                    }
                }
                else
                {
                    isListening = false;
                    isAborted = true;
                }
            }
            catch (IOException e)
            {
                System.err.println("MeshMC ABORT due to IO exception:");
                e.printStackTrace();
                return 1;
            }
            catch (ParseException e)
            {
                System.err.println("MeshMC ABORT due to PARSE exception:");
                e.printStackTrace();
                return 1;
            }
        }
        if(isAborted)
        {
            System.err.println("Launch aborted by MeshMC.");
            return 1;
        }
        if(m_launcher != null)
        {
            return m_launcher.launch(m_params);
        }
        System.err.println("No valid launcher implementation specified.");
        return 1;
    }

    private ParamBucket m_params = new ParamBucket();
    private org.projecttick.MeshMC m_launcher;
}
