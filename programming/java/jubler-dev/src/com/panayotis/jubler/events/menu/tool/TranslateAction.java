/*
 *  TranslateAction.java 
 * 
 *  Created on: 20-Oct-2009 at 01:52:37
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
package com.panayotis.jubler.events.menu.tool;

import com.panayotis.jubler.Jubler;
import com.panayotis.jubler.MenuAction;
import com.panayotis.jubler.tools.JTranslate;
import java.awt.event.ActionEvent;

/**
 *
 * @author  teras
 */
public class TranslateAction extends MenuAction {

    private boolean directMode = false;

    public TranslateAction(Jubler parent, boolean is_direct) {
        super(parent);
        directMode = is_direct;
    }

    /**
     *
     * @param e Action Event
     */
    public void actionPerformed(ActionEvent evt) {
        Jubler jb = jublerParent;
        JTranslate translate = jb.getTranslate();
        if (this.directMode) {
            translate.performTranslation(jb.fn.getSelectedSubs());
            jb.fn.tableHasChanged(jb.fn.getSelectedSubs());
        } else {
            translate.execute(jb);
        }        
    }//end public void actionPerformed(ActionEvent evt)

    /**
     * @return the directMode
     */
    public boolean isDirectMode() {
        return directMode;
    }

    /**
     * @param directMode the directMode to set
     */
    public void setDirectMode(boolean directMode) {
        this.directMode = directMode;
    }
}//end public class TranslateAction extends MenuAction
