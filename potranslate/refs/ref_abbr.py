from matcher import MatcherRecord
from definition import TranslationState, Definitions as df
from refs.ref_base import RefBase

import re

class RefAbbr(RefBase):
    def getPattern(self):
        return df.ABBR

    def getTextForTranslate(self, entry):
        entry_loc = entry[0]
        mm: MatcherRecord = entry[1]

        sub_list = mm.getSubEntriesAsList()
        (oloc, orig) = sub_list[0]
        (abr_loc, abr_txt) = sub_list[1]
        (txt_loc, txt) = sub_list[2]
        return txt

    def parse(self, entry):
        entry_loc = entry[0]
        mm: MatcherRecord = entry[1]

        sub_list = mm.getSubEntriesAsList()
        (oloc, orig) = sub_list[0]
        (abr_loc, txt) = sub_list[1]
        (txt_loc, txt) = sub_list[2]

        tran = self.tf.isInDict(txt)
        has_tran = (tran is not None)
        if not has_tran:
            entry = (txt, "")
            self.statTranslation(orig=txt)
            tran = ""

        tran = self.extractAbbr(tran)
        tran = f'{txt}: {tran}'
        (os, oe) = oloc
        (ts, te) = txt_loc
        ns = ts - os
        ne = len(mm.txt) - (oe - te)
        new_loc = (ns, ne)

        new_tran = str(mm.txt)
        new_tran = self.jointText(new_tran, tran, new_loc)
        mm.translation = new_tran
        mm.translation_state = TranslationState.ACCEPTABLE
        self.statTranslation(matcher=mm)
        return entry