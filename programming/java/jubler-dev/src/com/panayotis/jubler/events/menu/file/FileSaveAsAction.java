/*
 *  FileSaveAsAction.java
 * 
 *  Created on: 20-Oct-2009 at 00:41:37
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

import com.panayotis.jubler.io.JublerFileChooser;
import com.panayotis.jubler.subs.SubFile;
import java.io.File;
import com.panayotis.jubler.Jubler;
import com.panayotis.jubler.MenuAction;
import com.panayotis.jubler.io.SimpleFileFilter;
import com.panayotis.jubler.media.MediaFile;
import com.panayotis.jubler.os.DEBUG;
import com.panayotis.jubler.os.FileCommunicator;
import com.panayotis.jubler.subs.Subtitles;
import com.panayotis.jubler.subs.loader.SubFormat;
import java.awt.event.ActionEvent;
import javax.swing.JFileChooser;
import static com.panayotis.jubler.i18n.I18N._;

/**
 *
 * @author  teras
 */
public class FileSaveAsAction extends MenuAction {

    public FileSaveAsAction(Jubler parent) {
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

        JublerFileChooser fd = jb.getFiledialog();
        SubFormat handler = null;
        File cur_f, last_f;
        try {
            cur_f = subs.getCurrentFile();
            last_f = subs.getLastOpenedFile();
            
            fd.setDialogTitle(_("Save Subtitles"));            
            fd.setSelectedFile(cur_f);
            if (fd.showSaveDialog(jb) != JFileChooser.APPROVE_OPTION) {
                return;
            }
            FileCommunicator.setDefaultDialogPath(fd);
            
            try {
                SimpleFileFilter flt = (SimpleFileFilter) fd.getFileFilter();
                handler = flt.getFormatHandler().newInstance();
            } catch (Exception ex) {
                handler = Jubler.prefs.getSaveFormat().newInstance();
            }            
            Jubler.prefs.getJsave().setSelectedFormat(handler);
            Jubler.prefs.showSaveDialog(jb, mfile, subs); //Show the "save options" dialog, if desired

            last_f = cur_f;
            cur_f = fd.getSelectedFile();
            
            SubFile sf = new SubFile(cur_f, last_f, handler);
            jb.getFileManager().saveFile(sf);
            jb.fn.changeTableRowHeightForTextTypeSubs();
        } catch (Exception ex) {
            DEBUG.debug(ex.toString());
        }
    }//end public void actionPerformed(ActionEvent evt)
}//end public class FileSaveAsAction extends MenuAction

