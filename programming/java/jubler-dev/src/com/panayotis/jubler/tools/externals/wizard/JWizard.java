/*
 * JWizard.java
 *
 * Created on June 2, 2007, 9:11 PM
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

package com.panayotis.jubler.tools.externals.wizard;

import static com.panayotis.jubler.i18n.I18N._;

import com.panayotis.jubler.options.JExtBasicOptions;
import com.panayotis.jubler.os.SystemDependent;
import com.panayotis.jubler.os.TreeWalker;
import java.awt.CardLayout;
import java.awt.Frame;
import java.io.File;
import javax.swing.JDialog;
import javax.swing.JFileChooser;

/**
 *
 * @author  teras
 */
public class JWizard extends JDialog {
    
    private int cardid = 1;
    private JExtBasicOptions ext;
    
    private String name;
    private String[] testparameters;
    private String test_signature;
    private String deflt;
    
    private JFileChooser fdialog;
    
    /** Creates new form JWizard */
    public JWizard(String name, String[] testparameters, String test_signature, String deflt) {
        super((Frame)null, true);
        
        this.name = name;
        this.deflt = deflt;
        this.testparameters = testparameters;
        this.test_signature = test_signature;
        
        fdialog = new JFileChooser();
        fdialog.setFileSelectionMode(JFileChooser.FILES_ONLY);
        
        initComponents();
        this.setLocationRelativeTo(null);
    }
    
    /** This method is called from within the constructor to
     * initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is
     * always regenerated by the Form Editor.
     */
    // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
    private void initComponents() {

        AutoSel = new javax.swing.ButtonGroup();
        CardsP = new javax.swing.JPanel();
        WelcomeP = new javax.swing.JPanel();
        jPanel2 = new javax.swing.JPanel();
        WelcomeTitle = new javax.swing.JLabel();
        jPanel1 = new javax.swing.JPanel();
        AutoSelL = new javax.swing.JLabel();
        AutoB = new javax.swing.JRadioButton();
        ManualB = new javax.swing.JRadioButton();
        WelcomeText = new javax.swing.JTextArea();
        AutoP = new javax.swing.JPanel();
        jPanel3 = new javax.swing.JPanel();
        AutoTitle = new javax.swing.JLabel();
        AutoL = new javax.swing.JLabel();
        AutoProgress = new javax.swing.JProgressBar();
        BrowseP = new javax.swing.JPanel();
        jPanel4 = new javax.swing.JPanel();
        BrowseTitle = new javax.swing.JLabel();
        FilenameT = new javax.swing.JTextField();
        BrowseB = new javax.swing.JButton();
        BrowseStatusL = new javax.swing.JLabel();
        FinishP = new javax.swing.JPanel();
        FInishTitle = new javax.swing.JLabel();
        LowerP = new javax.swing.JPanel();
        ButtonsP = new javax.swing.JPanel();
        CancelB = new javax.swing.JButton();
        ContinueB = new javax.swing.JButton();
        jLabel1 = new javax.swing.JLabel();

        setDefaultCloseOperation(javax.swing.WindowConstants.DO_NOTHING_ON_CLOSE);
        setTitle(_("External plugin Wizard"));
        setModal(true);
        setResizable(false);

        CardsP.setBorder(javax.swing.BorderFactory.createEmptyBorder(20, 30, 20, 20));
        CardsP.setLayout(new java.awt.CardLayout());

        WelcomeP.setLayout(new java.awt.BorderLayout());

        jPanel2.setLayout(new java.awt.BorderLayout());

        WelcomeTitle.setFont(new java.awt.Font("Lucida Grande", 1, 14));
        WelcomeTitle.setHorizontalAlignment(javax.swing.SwingConstants.LEFT);
        WelcomeTitle.setText(_("{0} executable was not found.", name));
        WelcomeTitle.setBorder(javax.swing.BorderFactory.createEmptyBorder(0, 3, 12, 0));
        WelcomeTitle.setHorizontalTextPosition(javax.swing.SwingConstants.LEFT);
        jPanel2.add(WelcomeTitle, java.awt.BorderLayout.NORTH);

        jPanel1.setBorder(javax.swing.BorderFactory.createEmptyBorder(8, 2, 0, 0));
        jPanel1.setLayout(new java.awt.GridLayout(0, 1));

        AutoSelL.setText(_("How should this issue been resolved?"));
        AutoSelL.setBorder(javax.swing.BorderFactory.createEmptyBorder(8, 2, 0, 0));
        jPanel1.add(AutoSelL);

        AutoSel.add(AutoB);
        AutoB.setSelected(true);
        AutoB.setText(_("Automatically search for the executable"));
        AutoB.setMargin(new java.awt.Insets(0, 0, 0, 0));
        jPanel1.add(AutoB);

        AutoSel.add(ManualB);
        ManualB.setText(_("Manually browse for the executable"));
        ManualB.setMargin(new java.awt.Insets(0, 0, 0, 0));
        jPanel1.add(ManualB);

        jPanel2.add(jPanel1, java.awt.BorderLayout.SOUTH);

        WelcomeText.setBackground(javax.swing.UIManager.getDefaults().getColor("Label.background"));
        WelcomeText.setColumns(20);
        WelcomeText.setEditable(false);
        WelcomeText.setFont(new java.awt.Font("Lucida Grande", 0, 14)); // NOI18N
        WelcomeText.setRows(3);
        WelcomeText.setText(_("Jubler needs {0} executable\nto continue with the requested action.", name));
        jPanel2.add(WelcomeText, java.awt.BorderLayout.CENTER);

        WelcomeP.add(jPanel2, java.awt.BorderLayout.NORTH);

        CardsP.add(WelcomeP, "card1");

        AutoP.setLayout(new java.awt.BorderLayout());

        jPanel3.setLayout(new java.awt.BorderLayout());

        AutoTitle.setFont(new java.awt.Font("Lucida Grande", 1, 14));
        AutoTitle.setHorizontalAlignment(javax.swing.SwingConstants.LEFT);
        AutoTitle.setText(_("Automatic location of {0} executable", name));
        AutoTitle.setBorder(javax.swing.BorderFactory.createEmptyBorder(0, 3, 12, 0));
        AutoTitle.setHorizontalTextPosition(javax.swing.SwingConstants.LEFT);
        jPanel3.add(AutoTitle, java.awt.BorderLayout.NORTH);

        AutoL.setText(_("Trying to locate {0} executable...", name));
        AutoL.setBorder(javax.swing.BorderFactory.createEmptyBorder(2, 0, 12, 0));
        jPanel3.add(AutoL, java.awt.BorderLayout.CENTER);

        AutoProgress.setIndeterminate(true);
        jPanel3.add(AutoProgress, java.awt.BorderLayout.SOUTH);

        AutoP.add(jPanel3, java.awt.BorderLayout.NORTH);

        CardsP.add(AutoP, "card2");

        BrowseP.setLayout(new java.awt.BorderLayout());

        jPanel4.setLayout(new java.awt.BorderLayout());

        BrowseTitle.setFont(new java.awt.Font("Lucida Grande", 1, 14));
        BrowseTitle.setHorizontalAlignment(javax.swing.SwingConstants.LEFT);
        BrowseTitle.setText(_("Manual selection of {0} executable.", name));
        BrowseTitle.setBorder(javax.swing.BorderFactory.createEmptyBorder(0, 3, 12, 0));
        BrowseTitle.setHorizontalTextPosition(javax.swing.SwingConstants.LEFT);
        jPanel4.add(BrowseTitle, java.awt.BorderLayout.NORTH);

        FilenameT.setColumns(20);
        FilenameT.setEditable(false);
        FilenameT.setToolTipText(_("The absolute path of the player. Use the Browse button to change it"));
        jPanel4.add(FilenameT, java.awt.BorderLayout.CENTER);

        BrowseB.setText(_("Browse"));
        BrowseB.setToolTipText(_("Open a file dialog to select the filename of the player"));
        BrowseB.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                BrowseBActionPerformed(evt);
            }
        });
        jPanel4.add(BrowseB, java.awt.BorderLayout.EAST);

        BrowseStatusL.setForeground(java.awt.Color.red);
        BrowseStatusL.setText(_("The selected file is not valid"));
        BrowseStatusL.setBorder(javax.swing.BorderFactory.createEmptyBorder(8, 4, 0, 0));
        BrowseStatusL.setVisible(false);
        jPanel4.add(BrowseStatusL, java.awt.BorderLayout.SOUTH);

        BrowseP.add(jPanel4, java.awt.BorderLayout.NORTH);

        CardsP.add(BrowseP, "card3");

        FinishP.setLayout(new java.awt.BorderLayout());

        FInishTitle.setFont(new java.awt.Font("Lucida Grande", 1, 14));
        FInishTitle.setHorizontalAlignment(javax.swing.SwingConstants.LEFT);
        FInishTitle.setText(_("{0} executable has been resolved", name));
        FInishTitle.setBorder(javax.swing.BorderFactory.createEmptyBorder(0, 3, 12, 0));
        FInishTitle.setHorizontalTextPosition(javax.swing.SwingConstants.LEFT);
        FinishP.add(FInishTitle, java.awt.BorderLayout.NORTH);

        CardsP.add(FinishP, "card4");

        getContentPane().add(CardsP, java.awt.BorderLayout.CENTER);

        LowerP.setBorder(javax.swing.BorderFactory.createEmptyBorder(1, 1, 8, 12));
        LowerP.setLayout(new java.awt.BorderLayout());

        ButtonsP.setLayout(new java.awt.GridLayout(1, 2, 5, 0));

        CancelB.setText(_("Cancel"));
        CancelB.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                CancelBActionPerformed(evt);
            }
        });
        ButtonsP.add(CancelB);

        ContinueB.setText(_("Continue"));
        ContinueB.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                ContinueBActionPerformed(evt);
            }
        });
        ButtonsP.add(ContinueB);

        LowerP.add(ButtonsP, java.awt.BorderLayout.EAST);

        getContentPane().add(LowerP, java.awt.BorderLayout.SOUTH);

        jLabel1.setIcon(new javax.swing.ImageIcon(getClass().getResource("/icons/wizard.jpg"))); // NOI18N
        getContentPane().add(jLabel1, java.awt.BorderLayout.WEST);

        pack();
    }// </editor-fold>//GEN-END:initComponents
    
    private void autoFind() {
        Thread auto = new Thread() {
            public void run() {
                File f = TreeWalker.searchExecutable(name, testparameters, test_signature, deflt);
                
                ContinueB.setEnabled(true);
                AutoProgress.setVisible(false);
                if (f==null) {
                    AutoL.setText(_("Unable to find executable"));
                    CancelB.setEnabled(true);
                } else {
                    FilenameT.setText(f.getPath());
                    clickContinue();
                }
            }
        };
        auto.start();
    }
    
    
    private void clickContinue() {
        switch (cardid) {
            case 1:
                if (ManualB.isSelected()) {
                    cardid++;
                    ContinueB.setEnabled(TreeWalker.execIsValid(new File(deflt), testparameters, test_signature, name.toLowerCase()));
                    FilenameT.setText(deflt);
                } else {
                    CancelB.setEnabled(false);
                    ContinueB.setEnabled(false);
                    autoFind();
                }
                break;
            case 2:
                if (FilenameT.getText().equals("")) {
                    ContinueB.setEnabled(false);
                    break;
                }
                cardid++;
                CancelB.setEnabled(false);
                ContinueB.setText(_("Finish"));
                break;
            case 3:
                CancelB.setEnabled(false);
                ContinueB.setText(_("Finish"));
                break;
            case 4:
                setVisible(false);
        }
        ((CardLayout)CardsP.getLayout()).show(CardsP,"card"+(++cardid));
    }
    
    
    private void BrowseBActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_BrowseBActionPerformed
        BrowseStatusL.setVisible(false);
        if ( fdialog.showOpenDialog(this) != JFileChooser.APPROVE_OPTION) return;
        File newexe = TreeWalker.searchExecutable(fdialog.getSelectedFile(), name.toLowerCase(), testparameters, test_signature, SystemDependent.getBundleOrFileID());
        if (newexe!=null) {
            FilenameT.setText(newexe.getPath());
            ContinueB.setEnabled(true);
        } else {
            BrowseStatusL.setVisible(true);
            FilenameT.setText("");
            ContinueB.setEnabled(false);
        }
    }//GEN-LAST:event_BrowseBActionPerformed
    
    private void ContinueBActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_ContinueBActionPerformed
        clickContinue();
    }//GEN-LAST:event_ContinueBActionPerformed
    
    private void CancelBActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_CancelBActionPerformed
        FilenameT.setText("");
        setVisible(false);
    }//GEN-LAST:event_CancelBActionPerformed
    
    public String getExecFilename() {
        if (FilenameT.getText().equals("")) return null;
        else return FilenameT.getText();
    }
    
    // Variables declaration - do not modify//GEN-BEGIN:variables
    private javax.swing.JRadioButton AutoB;
    private javax.swing.JLabel AutoL;
    private javax.swing.JPanel AutoP;
    private javax.swing.JProgressBar AutoProgress;
    private javax.swing.ButtonGroup AutoSel;
    private javax.swing.JLabel AutoSelL;
    private javax.swing.JLabel AutoTitle;
    private javax.swing.JButton BrowseB;
    private javax.swing.JPanel BrowseP;
    private javax.swing.JLabel BrowseStatusL;
    private javax.swing.JLabel BrowseTitle;
    private javax.swing.JPanel ButtonsP;
    private javax.swing.JButton CancelB;
    private javax.swing.JPanel CardsP;
    private javax.swing.JButton ContinueB;
    private javax.swing.JLabel FInishTitle;
    private javax.swing.JTextField FilenameT;
    private javax.swing.JPanel FinishP;
    private javax.swing.JPanel LowerP;
    private javax.swing.JRadioButton ManualB;
    private javax.swing.JPanel WelcomeP;
    private javax.swing.JTextArea WelcomeText;
    private javax.swing.JLabel WelcomeTitle;
    private javax.swing.JLabel jLabel1;
    private javax.swing.JPanel jPanel1;
    private javax.swing.JPanel jPanel2;
    private javax.swing.JPanel jPanel3;
    private javax.swing.JPanel jPanel4;
    // End of variables declaration//GEN-END:variables
    
}
