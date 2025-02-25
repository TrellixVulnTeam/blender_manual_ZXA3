#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import division

import re
from docutils import nodes
import sys
from sphinx.cmd.build import main
from definition import Definitions as df
from sphinx.util.nodes import extract_messages
from translation_finder import TranslationFinder
from reflist import RefList

tf = TranslationFinder()

def doctree_resolved(app, doctree, docname):
    def getRefList(txt):
        ref_list = RefList(msg=txt, keep_orig=False, tf=tf)
        count = ref_list.findPattern(df.pattern_list, txt)
        return ref_list, count
        
    def isKeepCopyOfOriginal(node):
        is_inline = isinstance(node, nodes.inline)
        is_emphasis = isinstance(node, nodes.emphasis)
        is_title = isinstance(node, nodes.title)
        is_term = isinstance(node, nodes.term)
        is_rubric = isinstance(node, nodes.rubric)
        is_field_name = isinstance(node, nodes.field_name)
        is_reference = isinstance(node, nodes.reference)
        is_strong = isinstance(node, nodes.strong)

        is_keep_original = (is_inline or
                            is_emphasis or
                            is_title or
                            is_term or
                            is_rubric or
                            is_field_name or
                            is_reference or
                            is_strong
                            )
        return is_keep_original

    try:
        changed = False
        for node, msg in extract_messages(doctree):
            # result_list = []
            last_char = msg[-1]
            is_ending_stop = (last_char == '.')

            ref_list, count = getRefList(msg)
            has_ref = (count > 0)

            is_supposed_to_repeat = isKeepCopyOfOriginal(node)
            is_repeat = (is_supposed_to_repeat and not is_ending_stop)
            is_check_tran = (is_repeat or has_ref)
            if not is_check_tran:
                continue

            tran = tf.isInDict(msg)
            has_tran = bool(tran)
            if not has_tran:
                continue


            # ref_list = RefList(msg=msg, keep_orig=False, tf=tf)
            # ref_list.parseMessage(is_ref_only=True)
            # list_msg_to_check = ref_list.getComponentTexts()

            # if list_msg_to_check:
            #     result_list.extend(list_msg_to_check)
            #
            # is_included = (msg not in list_msg_to_check)
            # if is_included:
            #    result_list.append(msg)

            # list_msg_to_check = []
            # if is_repeat:
            #     msg = msg.strip()
            #     list_msg_to_check.append(msg)
            # elif has_ref:
            #     mm: MatcherRecord = None
            #     for loc, mm in ref_list.items():
            #         ref_txt = mm.txt
            #         list_msg_to_check.append(ref_txt)

            # print('-' * 80)
            # print(result_list)

            dict_list=[]
            for msg in result_list:
                tran = tf.isInDict(msg)
                if not tran:
                    dict_list.append(msg)
                    tf.addBackupDictEntry(msg, "")
                    changed = True
        if changed:
            tf.writeChosenDict(is_master=False)
            changed = False

    except Exception as e:
        df.LOG(f'{e}', error=False)

def build_finished(app, exeption):
    # tf.writeChosenDict(is_master=False)
    pass

def setup(app):
    # app.connect('builder-inited', builder_inited)
    app.connect('doctree-resolved', doctree_resolved)
    app.connect('build-finished', build_finished)
    # app.connect('env-updated', env_updated)
    # env-updated

    return {
        'version': '0.1',
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }


if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw?|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
