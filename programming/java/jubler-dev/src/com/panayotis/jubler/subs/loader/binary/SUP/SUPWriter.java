/*
 *  SUPWriter.java 
 * 
 *  Created on: Jul 19, 2009 at 12:25:50 PM
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
package com.panayotis.jubler.subs.loader.binary.SUP;

import com.panayotis.jubler.Jubler;
import com.panayotis.jubler.subs.loader.binary.*;
import static com.panayotis.jubler.subs.style.StyleType.*;
import com.panayotis.jubler.subs.Share;
import com.panayotis.jubler.subs.SubEntry;
import com.panayotis.jubler.subs.Subtitles;
import com.panayotis.jubler.subs.color.ReduceColorDepth;
import com.panayotis.jubler.subs.loader.ImageTypeSubtitle;
import com.panayotis.jubler.subs.loader.binary.SON.record.SonSubEntry;
import com.panayotis.jubler.subs.loader.binary.SON.record.SubtitleImageAttribute;
import com.panayotis.jubler.time.Time;
import com.panayotis.jubler.tools.JImage;
import java.awt.Rectangle;
import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;

/**
 * See {@link SUPCompressImageProcessor} for descriptions.
 * @author Hoang Duy Tran <hoangduytran1960@googlemail.com>
 */
public class SUPWriter extends SUPCompressImageProcessor {

    /**
     * Parameterised constructor
     * @param jubler The main application's GUI screen
     * @param fps The number of frame per seconds chosen, ie. PAL uses 25fps
     * @param encoding The encoding scheme (ie. UTF-8)
     * @param f The output file to write SUP compressed images.
     */
    public SUPWriter(Jubler jubler, float fps, String encoding, File f) {
        super(jubler, fps, encoding);
        processFile = f;
    }

    /**
     * Checks to see if the entry is a image-type subtitle or not.
     * If it is, checks to see if it actually has an image. If not, generate
     * a new-blank image, to avoid the rest of the subtitle file being discarded
     * by the process. The entry is then casted to the 
     * {@link ImageTypeSubtitle} and return to the caller. <br><br>
     * When an entry is NOT a {@link ImageTypeSubtitle} entry, an instance of
     * SON subtitle entry is created and the subtitle text is converted to
     * an image. When no text is found, a blank string is used.
     * The subtitle image will have a blue background, and the
     * text image is over-laid in the centre. The default header is held
     * locally for next entries. 
     * @param entry The subtitle entry to be converted to an 
     * {@link ImageTypeSubtitle}.
     * @return the reference to an instance of {@link ImageTypeSubtitle}.
     */
    private ImageTypeSubtitle getImageTypeEntry(SubEntry entry) {
        ImageTypeSubtitle img_entry;
        boolean has_image = (entry instanceof ImageTypeSubtitle);
        boolean has_text = (!Share.isEmpty(entry.getText()));

        if (!has_image) {
            SonSubEntry son_entry = new SonSubEntry();
            son_entry.setHeader(son_header);
            son_entry.copyRecord(entry);

            BufferedImage img = null;
            if (has_text) {
                BufferedImage text_img = son_entry.makeSubtitleTextImage();
                son_entry.getCreateSonAttribute().centreImage(text_img);
                img = JImage.makeSubBackgroundImage(text_img);                
            } else {
                img = JImage.getDefaultBlankImage();                
            }
            son_entry.setImage(img);
            son_header = son_entry.getHeader();
            son_header.FPS = this.FPS;
            is_text = true;
            return son_entry;
        } else {
            img_entry = (ImageTypeSubtitle) entry;
            has_image = (img_entry.getImage() != null);
            if (!has_image) {
                BufferedImage img = JImage.getDefaultBlankImage();
                img_entry.setImage(img);
            }//end if
            return img_entry;
        }
    }//end private ImageTypeSubtitle isImageType(SubEntry entry)
    /**
     * This function will convert the image from the input instance of
     * {@link ImageTypeSubtitle} to an image with maximum of 4-colors.
     * This is due to the SUP requirements for RLE compression that the 
     * image will only have a maximum of 4 colors. Conversion is carried
     * out here to avoid color-cutoff that would happen in compression
     * stage, which would produce wrong color indices, resulting to a very
     * bad image when it is decoded. The pixel data of image is extracted
     * and held locally so that it can be passed to the compressor later-on.
     * The local user-color table is updated with the new 4 colors found,
     * either from the image or the color-palette list from the color-reducer.
     * @param img_entry The subtitle entry which contains image to be
     * compressed.
     */
    private void getSubtitleImageData(ImageTypeSubtitle img_entry) {
        BufferedImage img = img_entry.getImage();

        //down-sample the image to 4 colors
        ReduceColorDepth reducer = new ReduceColorDepth(img, 4);
        reducer.reduceColor();
        BufferedImage n_img = reducer.getReducedImage();

        //debugging code
        //JPanel pan = ds.getDebugPanel(n_img);
        //JOptionPane.showMessageDialog(null, pan);

        width = img.getWidth();
        height = img.getHeight();
        imageData = n_img.getRGB(0, 0, width, height, null, 0, width);
        updateUserColourTable(imageData);
    }//end private Rectangle getSubtitleImageData(ImageTypeSubtitle img_entry)
    /**
     * Creates an instance of {@link BitmapRLE} and pass it the local
     * image data, its width and height, and the color-table, before invokes
     * its compress method to carry out the RLE image-compression.
     * The compressed data is retrieved and set locally. 
     * @return True if the compression was carried out without problems,
     * false otherwise.
     */
    private boolean compressImageData() {
        boolean ok = false;
        brle = new BitmapRLE();
        brle.setWidth(width);
        brle.setHeight(height);
        brle.setUncompressedData(imageData);
        brle.setColorTable(color_table);
        ok = brle.compress();
        if (ok) {
            brle.makeTranparencyList();
            compressedImageData = brle.getCompressedData();
        } else {
            compressedImageData = null;
        }//end if (!ok)
        return ok;
    }//end private boolean compressImageData(Rectangle rec)
    /**
     * Sets the screen positions of the image on the screen in the 
     * control-section of the packet so that it can be written to the file.
     * @param minX Top left x co-ordinate.
     * @param minY Top left y co-ordinate.
     * @param maxX Bottom right x co-ordinate.
     * @param maxY Bottom right y co-ordinate.
     */
    private void setScreenPosition(int minX, int minY, int maxX, int maxY) {
        // set planned pic pos. on tvscreen
        sections[9] = (byte) (minX >>> 4);
        sections[10] = (byte) (minX << 4 | maxX >>> 8);
        sections[11] = (byte) maxX;
        sections[12] = (byte) (minY >>> 4);
        sections[13] = (byte) (minY << 4 | maxY >>> 8);
        sections[14] = (byte) maxY;
    }

    private void setControlBlockPosition(int control_block_pos, int bottom_field_start_pos) {
        // top_field
        sections[16] = 0;
        sections[17] = 4;

        // bottom_field
        sections[18] = (byte) (0xFF & bottom_field_start_pos >>> 8);
        sections[19] = (byte) (0xFF & bottom_field_start_pos);

        // control_block
        sections[0] = sections[24] = (byte) (0xFF & control_block_pos >>> 8);
        sections[1] = sections[25] = (byte) (0xFF & control_block_pos);
    }

    private void setPGCsection(BitmapRLE rle) {
        int pgc_values = setPGClinks(rle);

        // color index 3,2 + 1,0
        sections[3] = (byte) (0xFF & pgc_values >>> 8);
        sections[4] = (byte) (0xFF & pgc_values);

        // alpha index 3,2 + 1,0
        sections[6] = (byte) (0xFF & pgc_values >>> 24);
        sections[7] = (byte) (0xFF & pgc_values >>> 16);
    }

    public int setPGClinks(BitmapRLE rle) {
        int pgc_colors = 0xFE10;
        int pgc_alphas = 0xFFF9;        
        try {
            Object[] pgc_color_links;
            Object[] pgc_alpha_links;
            if (is_text == true) {
                pgc_color_links = new Object[]{"0", "1", "2", "3"};
                pgc_alpha_links = this.color_table.toArray();
            } else {
                pgc_color_links = rle.getPgcColorIndexList().toArray();
                pgc_alpha_links = rle.getColorTable().toArray();
            }//end if

            int pgc_color_value, pgc_alpha_value;
            int max_tbl_size = Math.min(pgc_alpha_links.length, pgc_color_links.length);
            for (int a = 0; a < 4; a++) {
                if (a < max_tbl_size) {
                    pgc_color_value = 0xF & Integer.parseInt(pgc_color_links[a].toString());
                    pgc_alpha_value = 0xF & Integer.parseInt(pgc_alpha_links[pgc_color_value].toString()) >>> 28;
                    pgc_colors = (pgc_colors & ~(0xF << (a * 4))) | pgc_color_value << (a * 4);
                    pgc_alphas = (pgc_alphas & ~(0xF << (a * 4))) | pgc_alpha_value << (a * 4);
                }
            }
        } catch (Exception ex) {
        }
        return (pgc_alphas << 16 | pgc_colors);
    }//end public int setPGClinks(BitmapRLE rle)
    /**
     * Runs through the subtitle list, compress the image and write it 
     * to a file.
     */
    public void writeSupFile() {
        FileOutputStream of = null;
        SubEntry entry;
        ImageTypeSubtitle img_entry;
        boolean ok = false;
        try {
            Subtitles sub_list = this.getSubList();
            int sub_len = sub_list.size();
            out = new ByteArrayOutputStream();
            for (int i = 0; i < sub_len; i++) {
                out.reset();
                out.write(RLEheader);

                //1. extract the subtitle entry's image data
                entry = sub_list.elementAt(i);
                img_entry = this.getImageTypeEntry(entry);
                if (Share.isEmpty(img_entry)) {
                    break;
                }//end if (Share.isEmpty(img_entry))

                getSubtitleImageData(img_entry);
                ok = compressImageData();
                if (!ok) {
                    break;
                }//end if (!ok)

                out.write(compressedImageData);
                //3. update the SUP data

                int pack = out.size() - 12;
                int control_block_pos = pack + 24;
                int onscreen_time_pos = out.size() + 22;

                int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                SubtitleImageAttribute attrib = img_entry.getImageAttribute();
                if (Share.isEmpty(attrib)) {
                    attrib = new SubtitleImageAttribute();
                    attrib.centreImage(img_entry.getImage());
                } else {
                    if (Share.isEmpty(attrib.display_area)) {
                        attrib.centreImage(img_entry.getImage());
                    }//end if
                }//end if (! Share.isEmpty(attrib))
                x1 = attrib.display_area[0];
                y1 = attrib.display_area[1];
                x2 = attrib.display_area[2];
                y2 = attrib.display_area[3];

                setScreenPosition(x1, y1, x2, y2);
                setControlBlockPosition(control_block_pos, brle.getBottomFieldStartPost() + 4);
                setPGCsection(brle);

                out.write(sections);  //write control_block

                if ((out.size() & 1) == 1) {
                    out.write((byte) 255);
                }

                out.flush();

                byte[] picture_packet = out.toByteArray();

                int size = picture_packet.length - 10;

                picture_packet[10] = (byte) (0xFF & size >>> 8);
                picture_packet[11] = (byte) (0xFF & size);
                picture_packet[12] = (byte) (0xFF & pack >>> 8);
                picture_packet[13] = (byte) (0xFF & pack);

                int end_time = entry.getFinishTime().getMilli();
                int start_time = entry.getStartTime().getMilli();

                int play_duration = (end_time - start_time) / 10;
                float vfr = (FPS == 25f ? Time.PAL_VIDEOFRAMERATE : Time.NTSC_VIDEOFRAMERATE);
                int in_frm_cnt =
                        entry.getStartTime().getFrameCount(vfr);

                //Time starting_time = new Time(in_frm_cnt, (long) Time.PAL_VIDEOFRAMERATE);
                //Time finished_time = new Time(in_frm_cnt, (long) Time.PAL_VIDEOFRAMERATE, play_duration);
                //String starting_time_s = starting_time.toString();
                //String finished_time_s = finished_time.toString();

                picture_packet[2] = (byte) (0xff & in_frm_cnt);
                picture_packet[3] = (byte) (0xff & in_frm_cnt >>> 8);
                picture_packet[4] = (byte) (0xff & in_frm_cnt >>> 16);
                picture_packet[5] = (byte) (0xff & in_frm_cnt >>> 24);

                for (int a = 0; a < 4; a++) {
                    picture_packet[a + 2] = (byte) (0xFF & in_frm_cnt >>> (a * 8));
                }

                picture_packet[onscreen_time_pos] = (byte) (0xFF & play_duration >>> 8);
                picture_packet[onscreen_time_pos + 1] = (byte) (0xFF & play_duration);
                if (of == null) {
                    of = new FileOutputStream(this.processFile);
                }
                of.write(picture_packet);

                setEntry(entry);
                setRow(i);
                fireSubtitleRecordUpdatedEvent();

            }//end for(int i=0; i < sub_len; i++)

            String output_filename = this.processFile.getAbsolutePath();
            SUPIfo.createIfo(output_filename, color_table.toArray());

        } catch (Exception ex) {
        } finally {
            try {
                if (out != null) {
                    out.close();
                }
                if (of != null) {
                    of.flush();
                    of.close();
                }
            } catch (Exception e) {
            }
        }//end try/catch
    }//end public void writeSupFile()
    public void run() {
        SonSubEntry.reset();
        this.writeSupFile();
    }//end public void run()
}//end public class SUPWriter extends SUPCompressImageProcessor

