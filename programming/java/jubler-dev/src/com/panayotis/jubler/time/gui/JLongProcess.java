/*
 * JLongProcess.java
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

package com.panayotis.jubler.time.gui;

import java.awt.Frame;
import java.awt.event.ActionListener;
import static com.panayotis.jubler.i18n.I18N._;

/**
 *
 * @author  teras
 */
public class JLongProcess extends javax.swing.JDialog {
    
    private ActionListener listener;
    
    /** Creates new form JLongProcess */
    public JLongProcess(ActionListener listener) {
        super((Frame)null, true);
        this.listener = listener;
        initComponents();
        CancelB.setEnabled(listener!=null);
    }
    
    
    public void setValues(int size, String label) {
        setTitle(label);
        ProgBar.setMaximum(size);
        updateProgress(0);
        InfoL.setText(label);
        pack();
        setLocationRelativeTo(null);
    }
    
    public void updateProgress(int id) {
        ProgBar.setValue(id);
    }
      
    /** This method is called from within the constructor to
     * initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is
     * always regenerated by the Form Editor.
     */
    // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
    private void initComponents() {

        jPanel1 = new javax.swing.JPanel();
        InfoL = new javax.swing.JLabel();
        ProgBar = new javax.swing.JProgressBar();
        jPanel2 = new javax.swing.JPanel();
        jPanel3 = new javax.swing.JPanel();
        CancelB = new javax.swing.JButton();

        setDefaultCloseOperation(javax.swing.WindowConstants.DO_NOTHING_ON_CLOSE);
        setTitle(_("Save progress"));
        setResizable(false);

        jPanel1.setBorder(javax.swing.BorderFactory.createEmptyBorder(20, 20, 20, 20));
        jPanel1.setLayout(new java.awt.BorderLayout());
        jPanel1.add(InfoL, java.awt.BorderLayout.NORTH);

        ProgBar.setToolTipText(_("Save progress"));
        jPanel1.add(ProgBar, java.awt.BorderLayout.CENTER);

        jPanel2.setLayout(new java.awt.BorderLayout());

        CancelB.setText(_("Cancel"));
        CancelB.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                CancelBActionPerformed(evt);
            }
        });
        jPanel3.add(CancelB);

        jPanel2.add(jPanel3, java.awt.BorderLayout.EAST);

        jPanel1.add(jPanel2, java.awt.BorderLayout.SOUTH);

        getContentPane().add(jPanel1, java.awt.BorderLayout.CENTER);

        pack();
    }// </editor-fold>//GEN-END:initComponents

private void CancelBActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_CancelBActionPerformed
    if (listener!=null) {
        listener.actionPerformed(evt);
        CancelB.setEnabled(false);
    }
}//GEN-LAST:event_CancelBActionPerformed
    
 
    
    // Variables declaration - do not modify//GEN-BEGIN:variables
    private javax.swing.JButton CancelB;
    private javax.swing.JLabel InfoL;
    private javax.swing.JProgressBar ProgBar;
    private javax.swing.JPanel jPanel1;
    private javax.swing.JPanel jPanel2;
    private javax.swing.JPanel jPanel3;
    // End of variables declaration//GEN-END:variables
    
}
