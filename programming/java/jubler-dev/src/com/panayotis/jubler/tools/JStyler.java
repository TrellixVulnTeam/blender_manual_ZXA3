/*
 * JStyler.java
 *
 * Created on 26 Ιούνιος 2005, 12:59 πμ
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
 */

package com.panayotis.jubler.tools;
import com.panayotis.jubler.Jubler;

import static com.panayotis.jubler.i18n.I18N._;
import com.panayotis.jubler.subs.style.SubStyle;

/**
 *
 * @author  teras
 */
public class JStyler extends JTool {
    
    SubStyle style;
    
    public JStyler() {
        super(true);
    }
    
    public void initialize() {
        initComponents();
    }
    
    protected void updateData(Jubler jub) {
        super.updateData(jub);
        
        int selvalue = StyleSel.getSelectedIndex();
        StyleSel.removeAllItems();
        for ( SubStyle style : subs.getStyleList() )
            StyleSel.addItem(style);
        if ( selvalue < 0 )
            selvalue = 0;
        if ( selvalue < subs.getStyleList().size() )
            StyleSel.setSelectedIndex(selvalue);
    }
    
    protected String getToolTitle() {
        return _("Set region style");
    }
    
    protected void storeSelections() {
        style = (SubStyle)StyleSel.getSelectedItem();
    }
    
    protected void affect(int index) {
        affected_list.elementAt(index).setStyle(style);
    }
    
    /** This method is called from within the constructor to
     * initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is
     * always regenerated by the Form Editor.
     */
    // <editor-fold defaultstate="collapsed" desc=" Generated Code ">//GEN-BEGIN:initComponents
    private void initComponents() {
        JPanel2 = new javax.swing.JPanel();
        jLabel1 = new javax.swing.JLabel();
        StyleSel = new javax.swing.JComboBox();

        setLayout(new java.awt.BorderLayout());

        setToolTipText("Select the color to use in order to mark the area");
        JPanel2.setLayout(new java.awt.BorderLayout());

        jLabel1.setHorizontalAlignment(javax.swing.SwingConstants.RIGHT);
        jLabel1.setText(_("Style to use")+"  ");
        JPanel2.add(jLabel1, java.awt.BorderLayout.WEST);

        StyleSel.setToolTipText(_("Select the desired style from the drop down list"));
        JPanel2.add(StyleSel, java.awt.BorderLayout.CENTER);

        add(JPanel2, java.awt.BorderLayout.SOUTH);

    }
    // </editor-fold>//GEN-END:initComponents
    
    
    // Variables declaration - do not modify//GEN-BEGIN:variables
    private javax.swing.JPanel JPanel2;
    private javax.swing.JComboBox StyleSel;
    private javax.swing.JLabel jLabel1;
    // End of variables declaration//GEN-END:variables
    
}
