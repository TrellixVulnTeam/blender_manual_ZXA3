/*
 * 
 * SonHeader.java
 *  
 * Created on 06-Dec-2008, 00:14:44
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

import com.panayotis.jubler.subs.Share;
import com.panayotis.jubler.subs.loader.SubFormat;
import com.panayotis.jubler.subs.loader.binary.SON.SONPatternDef;
import com.panayotis.jubler.subs.loader.binary.SON.JMaestroOptions;
import java.io.File;
import java.util.ArrayList;
import java.util.Vector;


/**
 * This class is used to hold the header record of a SON subtitle format.
 * The example for this block of data in the subtitle file is shown here:
 * <pre>
 * st_format	2
 * Display_Start	non_forced
 * TV_Type		PAL
 * Tape_Type	NON_DROP
 * Pixel_Area	(0 575)
 * Directory	C:\java\test_data\edwardian
 * Contrast	( 15 0 15 15 )
 * Color	(0 1 6 7)
 * Display_Area	(000 446 720 518)
 *
 * #
 * # Palette entries:
 * #
 * # 00 : RGB(255,255, 0)
 * # 01 : RGB(131,127, 0)
 * # 02 : RGB( 8, 0, 0)
 * #
 *</pre>
 * @author Hoang Duy Tran <hoang_tran>
 */
public class SonHeader implements SONPatternDef, Cloneable {

    /**
     * this is to flag that the record is a default header generated and
     * further modification might be required. This is useful when conversion
     * from other record types are performed.
     */
    private boolean defaultHeader = false;
    public float FPS = 25f;
    public int st_format = -1;
    public String display_start = null;
    public String tv_type = null;
    public String tape_type = null;
    public short[] pixel_area = null;
    public SubtitleImageAttribute son_attribute = null;
    public Vector<SonPaletteEntry> palletEntry = null;
    public String image_directory = null;
    public File subtitle_file = null;
    public int max_row_height = -1;
    public JMaestroOptions moptions = null;
    public ArrayList<String> color_table = null;
    
    public Object getHeader() {
        return this;
    }

    public String getHeaderAsString() {
        return this.toString();
    }

    public void updateRowHeight(int height) {
        boolean is_taller = (max_row_height < height);
        if (is_taller) {
            max_row_height = height;
        }//end if

    }

    @Override
    public String toString() {
        String txt = null;

        boolean is_new = (st_format == -1);

        StringBuffer b = new StringBuffer();
        b.append("st_format").append("\t");
        b.append(is_new ? 2 : st_format);
        b.append(UNIX_NL);

        b.append("Display_Start").append("\t");
        b.append(is_new ? "non_forced" : display_start);
        b.append(UNIX_NL);

        b.append("TV_Type").append("\t");
        if (is_new) {
            try {
                tv_type = moptions.getVideoFormat();
            } catch (Exception ex) {
                if (this.FPS == 25f)
                    tv_type = "PAL";
                else
                    tv_type = "NTSC";
            }
        }
        b.append(tv_type);
        b.append(UNIX_NL);

        b.append("Tape_Type").append("\t");
        if (tv_type.equals("PAL")){
            b.append(is_new ? "NON_DROP" : tape_type);
        }else{
            b.append(is_new ? "DROP" : tape_type);
        }
        b.append(UNIX_NL);

        b.append("Pixel_Area").append("\t").append("(");
        if (is_new) {
            b.append("0 477");
        } else {
            b.append(pixel_area[0]).append(" ").append(pixel_area[1]);
        }
        b.append(")").append(UNIX_NL);

        b.append("Directory").append("\t");
        b.append(image_directory == null ? this.subtitle_file.getParent() : image_directory).append(UNIX_NL);

        short w, h;
        if (is_new) {
            son_attribute = new SubtitleImageAttribute();
            try {
                w = (short) moptions.getVideoWidth();
                h = (short) moptions.getVideoHeight();
            } catch (Exception ex) {
                w = 720;
                h = 576;
            }
            
            son_attribute.display_area = new short[]{0, 0, w, h};
            son_attribute.colour = new short[]{0, 1, 0, 0};
            son_attribute.contrast = new short[]{15, 15, 15, 0};
        }

        boolean has_attribute = (son_attribute != null);
        if (has_attribute) {
            txt = son_attribute.toString();
            b.append(txt);
        }

        if (is_new) {
            addPaletteEntry((short) 0, 255, 255, 255);
            addPaletteEntry((short) 1, 64, 64, 64);
        }//end if (is_new)

        boolean has_palette = (this.palletEntry != null && this.palletEntry.size() > 0);
        if (has_palette) {
            b.append("#").append(UNIX_NL);
            b.append("# Palette entries:").append(UNIX_NL);
            b.append("#").append(UNIX_NL);
            for (int i = 0; i < this.palletEntry.size(); i++) {
                SonPaletteEntry spe = this.palletEntry.elementAt(i);
                b.append(spe.toString());
            }//end for
            b.append("#").append(UNIX_NL);
            b.append(UNIX_NL);
        }//end if (has_palette)

        addDetailHeader(b);
        return b.toString();
    }

    public StringBuffer addDetailHeader(StringBuffer b) {
        b.append(UNIX_NL);
        b.append(SONPatternDef.sonSubtitleEventHeaderLine).append(UNIX_NL);
        return b;
    }

    public Vector<SonPaletteEntry> getPalletEntry() {
        return palletEntry;
    }

    public void setPalletEntry(Vector<SonPaletteEntry> palletEntry) {
        this.palletEntry = palletEntry;
    }

    public void addPaletteEntry(String index_s, String r_s, String g_s, String b_s) {
        short index = SubFormat.parseShort(index_s);
        int r = SubFormat.parseInt(r_s);
        int g = SubFormat.parseInt(g_s);
        int b = SubFormat.parseInt(b_s);

        addPaletteEntry(index, r, g, b);
    }

    public void addPaletteEntry(short index, int r, int g, int b) {
        boolean is_create_new = (getPalletEntry() == null);
        if (is_create_new) {
            Vector<SonPaletteEntry> pel = new Vector<SonPaletteEntry>();
            setPalletEntry(pel);
        }//end if (is_create_new)

        SonPaletteEntry spe = new SonPaletteEntry(index, r, g, b);
        getPalletEntry().add(spe);
    }

    @Override
    @SuppressWarnings("unchecked")
    public Object clone() {
        SonHeader new_header = null;
        try {
            new_header = (SonHeader) super.clone();
            new_header.defaultHeader = this.defaultHeader;
            new_header.FPS = this.FPS;
            new_header.st_format = this.st_format;
            new_header.display_start = (display_start == null ? null : new String(display_start));
            new_header.tv_type = (tv_type == null ? null : new String(tv_type));
            new_header.tape_type = (tape_type == null ? null : new String(tape_type));
            new_header.pixel_area = Share.copyShortArray(pixel_area);
            new_header.son_attribute = (son_attribute == null ? null : (SubtitleImageAttribute) son_attribute.clone());
            new_header.palletEntry = (palletEntry == null ? null : (Vector<SonPaletteEntry>) palletEntry.clone());
            new_header.image_directory = (image_directory == null ? null : new String(image_directory));
            new_header.subtitle_file = subtitle_file;
            new_header.max_row_height = max_row_height;
            new_header.moptions = moptions;
            new_header.color_table = (color_table == null ? null : (ArrayList<String>)color_table.clone());
        } catch (Exception ex) {
        }
        return new_header;
    }

    @SuppressWarnings("unchecked")
    public void copyRecord(SonHeader o) {
        try {
            this.defaultHeader = o.defaultHeader;
            this.FPS = o.FPS;
            this.st_format = o.st_format;
            this.display_start = (o.display_start == null ? null : new String(o.display_start));
            this.tv_type = (o.tv_type == null ? null : new String(o.tv_type));
            this.tape_type = (o.tape_type == null ? null : new String(o.tape_type));
            this.pixel_area = Share.copyShortArray(o.pixel_area);
            this.son_attribute = (o.son_attribute == null ? null : (SubtitleImageAttribute) o.son_attribute.clone());
            this.palletEntry = (o.palletEntry == null ? null : (Vector<SonPaletteEntry>) o.palletEntry.clone());
            this.image_directory = (o.image_directory == null ? null : new String(o.image_directory));
            this.subtitle_file = o.subtitle_file;
            this.max_row_height = o.max_row_height;
            this.moptions = o.moptions;
            this.color_table = (o.color_table == null ? null : (ArrayList<String>)o.color_table.clone());
        } catch (Exception ex) {
        }
    }

    public void makeDefaultHeader() {
        this.defaultHeader = true;
        this.FPS = 25;
        this.st_format = 2;
        this.display_start = "non_forced";
        this.tv_type = "PAL";
        this.tape_type = "NON_DROP";
        this.pixel_area = new short[]{0, 575};
        this.son_attribute = new SubtitleImageAttribute();
        this.son_attribute.makeDefaulRecord();
        this.palletEntry = null;
        this.image_directory = USER_CURRENT_DIR;
        this.subtitle_file = new File(USER_CURRENT_DIR);
        this.max_row_height = 25;
        this.moptions = null;
        this.color_table = null;
    }

    /**
     * @return the defaultHeader
     */
    public boolean isDefaultHeader() {
        return defaultHeader;
    }

    /**
     * @param defaultHeader the defaultHeader to set
     */
    public void setDefaultHeader(boolean defaultHeader) {
        this.defaultHeader = defaultHeader;
    }

    public SubtitleImageAttribute getCreateSonAttribute() {
        if (this.son_attribute == null) {
            this.son_attribute = new SubtitleImageAttribute();
        }
        return this.son_attribute;
    }
    
}
