from matcher import MatcherRecord
from definition import TranslationState
from refs.ref_base import RefBase

import re

class RefWithLink(RefBase):

    def getPattern(self):
        pat = re.compile(r':\w+:\`([^\`\<\>]+)\s\<[^\`\<\>]+\>\`', re.I)
        return pat

    def parse(self, entry):
        entry_loc = entry[0]
        mm: MatcherRecord = entry[1]

        sub_list = mm.getSubEntriesAsList()
        (oloc, orig) = sub_list[0]
        (txt_loc, txt) = sub_list[1]

        tran = self.translateSingle(txt)
        has_translation = (tran is not None)
        if not has_translation:
            self.statTranslation(orig=txt)
            return entry
        else:
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