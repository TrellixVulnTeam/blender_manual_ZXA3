/*
 * 
 * SonSubEntry.java
 *  
 * Created on 06-Dec-2008, 00:16:54
 * 
 * This file is part of Jubler.
 * Jubler is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
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
package com.panayotis.jubler.subs.loader.binary.SON.record;

import com.panayotis.jubler.exceptions.IncompatibleRecordTypeException;
import com.panayotis.jubler.os.DEBUG;
import com.panayotis.jubler.subs.CommonDef;
import com.panayotis.jubler.subs.SubEntry;
import com.panayotis.jubler.subs.loader.HeaderedTypeSubtitle;
import com.panayotis.jubler.subs.loader.ImageTypeSubtitle;
import com.panayotis.jubler.subs.loader.binary.SON.DVDMaestro;
import com.panayotis.jubler.time.Time;
import com.panayotis.jubler.tools.JImage;
import java.awt.image.BufferedImage;
import java.io.File;
import java.text.NumberFormat;
import java.util.logging.Level;
import javax.swing.ImageIcon;

/**
 * This class is ued to hold the following parsed information found
 * within the SON subtitle file. This is the subtitle event which shows
 * the event-id, start time, finish time, and the subtitle-image file which
 * holds the subtitle text in a bit-map image. The image should be OCR(ed)
 * to get back the actual editable text. An example of such entry is shown
 * here:
 * <pre>
 *  0001		00:00:11:01	00:00:15:08	Edwardians In Colour _st00001p1.bmp
 * </pre>
 * @author Hoang Duy Tran <hoang_tran>
 */
public class SonSubEntry extends SubEntry implements ImageTypeSubtitle, HeaderedTypeSubtitle, CommonDef {

    public int max_digits = 4;
    public SonHeader header = null;
    public short event_id = 0;
    public SubtitleImageAttribute son_attribute = null;
    public String image_filename = null;
    private File imageFile = null;
    private BufferedImage image = null;
    private int maxImageHeight = 0;
    private ImageIcon ico = null;
    
    public int getMaxImageHeight() {
        return maxImageHeight;
    }

    public void setMaxImageHeight(int value) {
        if (value > maxImageHeight) {
            maxImageHeight = value;
        }
    }

    public BufferedImage getImage() {
        return image;
    }

    public static short[] makeAttributeEntry(String[] matched_data) {
        short[] array = new short[4];
        array[0] = DVDMaestro.parseShort(matched_data[0]);
        array[1] = DVDMaestro.parseShort(matched_data[1]);
        array[2] = DVDMaestro.parseShort(matched_data[2]);
        array[3] = DVDMaestro.parseShort(matched_data[3]);
        return array;
    }

    public static String shortArrayToString(short[] a, String title) {
        StringBuffer b = new StringBuffer();
        if (a != null && a.length > 3) {
            b.append(title).append("\t").append("(");
            b.append(a[0] + " " + a[1] + " " + a[2] + " " + a[3]);
            b.append(")").append(UNIX_NL);
            return b.toString();
        } else {
            return null;
        }
    }

    public String toString() {
        NumberFormat fmt = NumberFormat.getInstance();
        StringBuffer b = new StringBuffer();
        String txt = null;
        try {
            if (son_attribute != null) {
                txt = son_attribute.toString();
                b.append(txt);
            }

            fmt.setMinimumIntegerDigits(max_digits);
            fmt.setMaximumIntegerDigits(max_digits);
            fmt.setGroupingUsed(false);
            String leading_zeros_id = fmt.format(event_id);
            b.append(leading_zeros_id);
            b.append("\t\t");


            Time st = getStartTime();
            if (st != null) {
                txt = st.getSecondsFrames(header.FPS);
                b.append(txt).append(" ");
            }

            Time ft = getFinishTime();
            if (ft != null) {
                txt = ft.getSecondsFrames(header.FPS);
                b.append(txt).append(" ");
            }

            if (imageFile != null) {
                txt = imageFile.getName();
                b.append(txt).append(UNIX_NL);
            }
        } catch (Exception ex) {
            DEBUG.logger.log(Level.WARNING, ex.toString());
        }
        return b.toString();
    }

    @Override
    public Object clone() {
        SonSubEntry new_object = null;
        try {
            new_object = (SonSubEntry) super.clone();
            new_object.max_digits = max_digits;
            new_object.header = (header == null ? null : (SonHeader) header.clone());
            new_object.event_id = event_id;
            new_object.son_attribute = (this.son_attribute == null ? null : (SubtitleImageAttribute) son_attribute.clone());

            //avoid making copy of image as there aren't many option to alter its content
            //so make a shallow copy here for the time being.
            new_object.imageFile = imageFile;
            new_object.image = image;
        } catch (Exception ex) {
            DEBUG.logger.log(Level.WARNING, ex.toString());
        }
        return new_object;
    }

    public void copyRecord(SubEntry o) {
        SonSubEntry o_son = null;
        try {
            super.copyRecord(o);
            if (header == null) {
                SonHeader new_son_header = new SonHeader();
                try {
                    o_son = (SonSubEntry) o;
                    new_son_header.copyRecord(o_son.header);
                } catch (Exception ex) {
                    new_son_header.makeDefaultHeader();
                }
                header = new_son_header;
            }//end if

            o_son = (SonSubEntry) o;
            max_digits = o_son.max_digits;
            event_id = o_son.event_id;

            if (o_son.son_attribute != null) {
                this.son_attribute = (SubtitleImageAttribute) o_son.son_attribute.clone();
            }
            imageFile = o_son.imageFile;
            image = o_son.image;
        } catch (Exception ex) {
        }
    }//public void copyRecord(SubEntry o)
    public void setImage(BufferedImage image) {
        this.image = image;
        if (image != null) {
            int h = image.getHeight();
            setMaxImageHeight(h);
        }//end if (image != null)
    }

    public File getImageFile() {
        return imageFile;
    }

    public void setImageFile(File imageFile) {
        this.imageFile = imageFile;
    }

    public String getImageFileName() {
        return this.image_filename;
    }

    public void setImageFileName(String name) {
        this.image_filename = name;
    }

    public boolean cutImage() throws Exception {
        this.setImage(null);
        this.setImageFile(null);
        return true;
    }//end public boolean copyImage(SubEntry source)
    public boolean copyImage(SubEntry source) throws Exception {
        try {
            ImageTypeSubtitle source_img_sub = (ImageTypeSubtitle) source;
            setImage(source_img_sub.getImage());
            setImageFile(source_img_sub.getImageFile());
            return true;
        } catch (Exception ex) {
            throw new IncompatibleRecordTypeException(ex.getMessage());
        }
    }//end public boolean copyImage(SubEntry source)
    public SubtitleImageAttribute getCreateSonAttribute() {
        if (this.son_attribute == null) {
            this.son_attribute = new SubtitleImageAttribute();
        }
        return this.son_attribute;
    }

    public SubtitleImageAttribute getImageAttribute() {
        return this.son_attribute;
    }

    public void setImageAttribute(SubtitleImageAttribute attrib) {
        this.son_attribute = attrib;
    }
    private static Object[] color_list = null;
    private static Object[] trans_list = null;

    public static void reset() {
        color_list = null;
        trans_list = null;
    }

    public BufferedImage makeTransparentImage(BufferedImage img) {
        try {
            SubtitleImageAttribute global_list = header.getCreateSonAttribute();
            if (color_list == null) {
                color_list = global_list.getColor();
            }
            if (trans_list == null) {
                trans_list = global_list.getContrast();
            }

            SubtitleImageAttribute local_list = getImageAttribute();
            boolean is_using_local_color =
                    !(local_list == null || local_list.colour == null);
            if (is_using_local_color) {
                color_list = local_list.getColor();
            }//end if

            boolean is_using_local_trans =
                    !(local_list == null || local_list.contrast == null);
            if (is_using_local_trans) {
                trans_list = local_list.getContrast();
            }//end if

            Object[] color_tbl = header.color_table.toArray();

            BufferedImage tran_img =
                    JImage.makeTransparentImage(img, color_list, trans_list, color_tbl);

            return tran_img;
        } catch (Exception ex) {
            return img;
        }
    }//end private SonSubEntry makeTransparentImage(BufferedImage img, SonSubEntry entry)
    public SonHeader getHeader() {
        return header;
    }

    public String getHeaderAsString() {
        if (header == null) {
            return "";
        } else {
            return header.getHeaderAsString();
        }
    }

    public void setHeader(Object header) {
        boolean ok = (header != null && (header instanceof SonHeader));
        if (ok) {
            this.header = (SonHeader) header;
        }
    }//public void setHeader(Object header)
    public Object getDefaultHeader() {
        SonHeader new_header = new SonHeader();
        new_header.makeDefaultHeader();
        return new_header;
    }

    public Object getData(int row, int col) {
        switch (col) {
            case 6:
                BufferedImage img = getImage();
                boolean has_image = (img != null);
                if (has_image) {
                    boolean is_translated = (ico != null);
                    if (! is_translated){
                        BufferedImage tran_image = this.makeTransparentImage(image);
                        ico = new ImageIcon(tran_image);
                    }//end if
                    return ico;
                } else {
                    return super.getData(row, col);
                }//end if (has_image)
            default:
                return super.getData(row, col);
        }//end switch/case
    }
}
