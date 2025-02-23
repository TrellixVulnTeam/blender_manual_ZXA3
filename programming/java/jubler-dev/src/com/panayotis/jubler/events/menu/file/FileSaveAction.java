/*
 *  FileSaveAction.java 
 * 
 *  Created on: 20-Oct-2009 at 01:36:55
 * 
 *  
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 * 
 * This file is part of Jubler.
 * 
 * Jubler is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 * 
 * 
 * Jubler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Jubler; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Contributor(s):
 * 
 */

package com.panayotis.jubler.events.menu.file;

import com.panayotis.jubler.Jubler;
import com.panayotis.jubler.MenuAction;
import com.panayotis.jubler.media.MediaFile;
import com.panayotis.jubler.subs.SubFile;
import com.panayotis.jubler.subs.Subtitles;
import java.awt.event.ActionEvent;
import java.io.File;

/**
 *
 * @author  teras
 */
public class FileSaveAction extends MenuAction {

    public FileSaveAction(Jubler parent) {
        super(parent);
    }

    /**
     *
     * @param e Action Event
     */
    public void actionPerformed(ActionEvent evt) {
        Jubler jb = jublerParent;
        Subtitles subs = jb.getSubtitles();
        MediaFile mfile = jb.getMediaFile();
        
        Jubler.prefs.showSaveDialog(jb, mfile, subs); //Show the "save options" dialog, if desired
        SubFile sf = subs.getSubfile();        
        jb.getFileManager().saveFile(sf);

    }//end public void actionPerformed(ActionEvent evt)
}//end public class FileSaveAction extends MenuAction
