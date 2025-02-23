import os
import re
from enum import Enum
# from urlextract import URLExtract as URLX
import utils as UT
import inspect as IP
import math
from utils import DEBUG


class OverLappingState(Enum):
    NONE = 0
    LEFT = 1
    RIGHT = 2
    BOTH = 3
    WITHIN = 4


class TranslationState(Enum):
    UNTRANSLATED = 0
    ACCEPTABLE = 1
    FUZZY = 2
    IGNORED = 3
    REMOVE = 4


class TextStyle(Enum):
    NORMAL = 0
    ITALIC = 1
    BOLD = 2
    BOX = 3
    RAW = 4


class RefType(Enum):
    GLOBAL = "None1"
    REMOVE_ORIGINAL = "None"
    PYTHON_FORMAT = "%"
    FUNCTION = "func"
    GA = "\`"
    GENERIC_QUOTE = "*'\""
    DOUBLE_GA = "``"
    SINGLE_GA = "`"
    GA_EMBEDDED_GA = "```"
    GA_LEADING_SYMBOLS = "``<-#"
    GA_INTERNAL_LINK = "``_"
    GA_EXTERNAL_LINK = "``__"
    GENERIC_DOUBLE_GA = "``"
    GENERIC_SINGLE_GA = "`"
    GENERIC_REF = ":\w+:"
    BLANK_QUOTE = "§"
    ARCH_BRACKET = "()"
    DBL_AST_QUOTE = "**"
    AST_QUOTE = "*"
    DBL_QUOTE = "\""
    SNG_QUOTE = "'"
    MM = ":MM:"
    ABBR = ":abbr:"
    CLASS = ":class:"
    DOC = ":doc:"
    GUILABEL = ":guilabel:"
    KBD = ":kbd:"
    LINENOS = ":linenos:"
    MATH = ":math:"
    MENUSELECTION = ":menuselection:"
    MOD = ":mod:"
    METHOD = ":meth:"
    FUNC = ":func:"
    REF = ":ref:"
    REF_WITH_LINK = ":ref:<>"
    SUP = ":sup:"
    TERM = ":term:"
    OSL_ATTRIB = "w:w"
    TEXT = "generic_text"
    FILLER = "filler"
    ATTRIB = "var:var"
    RESERVED = "reserved"

    @classmethod
    def getSearchableList(self):
        # searchable_list = [RefType.GENERIC_SINGLE_GA, RefType.GENERIC_DOUBLE_GA, RefType.REF]
        searchable_list = [RefType.GENERIC_REF]
        if not hasattr(self, 'searchable_list'):
            setattr(self, 'searchable_list', searchable_list)
        return getattr(self, 'searchable_list', searchable_list)

    @classmethod
    def getRef(self, string_value: str):
        def sortByValueLength(entry):
            (name, member) = entry
            return len(member.value)

        def compare(entry):
            (name, member) = entry
            string_lower = string_value.lower()
            member_value_lower = str(member.value).lower()
            is_found = (member_value_lower == string_lower) or (string_lower.startswith(member_value_lower))
            return is_found

        if not hasattr(self, 'member_list'):
            member_list = [(name, member) for (name, member) in self.__members__.items()]
            member_list.sort(key=sortByValueLength, reverse=True)
            setattr(self, 'member_list', member_list)

        found_list = list(filter(compare, self.member_list))
        if found_list:
            return found_list[0][1]
        else:
            return None


class Definitions:
    vn_uppercase_letters_and_digits = u"ẠĂÂÁẮẤÀẰẦẢẲẨÃẴẪẶẬĐÊÉẾÈỀẺỂẼỄẸỆỊÍÌỈĨỌÔƠÓỐỚÒỒỜỎỔỞÕỖỠỘỢỰƯÚỨÙỪỦỬŨỮÝỲỶỸỴA-Z\d"
    HOME = os.environ['HOME']
    log_path = os.path.join(HOME, 'Dev/tran/logme.log')

    LOG = UT.get_logger(log_path)

    KEYBOARD_TRANS_DIC = {
        r'\bWheelUp\b': "Lăn Bánh Xe về Trước (WheelUp)",
        r'\bWheelDown\b': "Lăn Bánh Xe về Sau (WheelDown)",
        r'\bWheel\b': 'Bánh Xe (Wheel)',
        "NumpadPlus": "Dấu Cộng (+) Bàn Số (NumpadPlus)",
        "NumpadMinus": "Dấu Trừ (-) Bàn Số (NumpadMinus)",
        "NumpadSlash": "Dấu Chéo (/) Bàn Số (NumpadSlash)",
        "NumpadDelete": "Dấu Xóa/Del Bàn Số (NumpadDelete)",
        "NumpadPeriod": "Dấu Chấm (.) Bàn Số (NumpadPeriod)",
        "Numpad0": "Số 0 Bàn Số (Numpad0)",
        "Numpad1": "Số 1 Bàn Số (Numpad1)",
        "Numpad2": "Số 2 Bàn Số (Numpad2)",
        "Numpad3": "Số 3 Bàn Số (Numpad3)",
        "Numpad4": "Số 4 Bàn Số (Numpad4)",
        "Numpad5": "Số 5 Bàn Số (Numpad5)",
        "Numpad6": "Số 6 Bàn Số (Numpad6)",
        "Numpad7": "Số 7 Bàn Số (Numpad7)",
        "Numpad8": "Số 8 Bàn Số (Numpad8)",
        "Numpad9": "Số 9 Bàn Số (Numpad9)",
        "Spacebar": "Dấu Cách (Spacebar)",
        r'\bDown\b': "Xuống (Down)",
        r'\bUp\b': "Lên (Up)",
        r'\bComma\b': "Dấu Phẩy (Comma)",
        r'\bMinus\b': "Dấu Trừ (Minus)",
        r'\bPlus\b': "Dấu Cộng (Plus)",
        "Left": "Trái (Left)",
        "=": "Dấu Bằng (=)",
        "Equals": "Dấu Bằng (=)",
        "Right": "Phải (Right)",
        "Backslash": "Dấu Chéo Ngược (Backslash)",
        "Back Space": "Phím Lùi",
        "BackSpace": "Phím Lùi",
        r'\bSlash\b': "Dấu Chéo (Slash)",
        "AccentGrave": "Dấu Huyền (AccentGrave)",
        "Delete": "Xóa (Delete)",
        "Period": "Dấu Chấm (Period)",
        "Comma": "Dấu Phẩy (Comma)",
        "PageDown": "Trang Xuống (PageDown)",
        "PageUp": "Trang Lên (PageUp)",
        "PgDown": "Trang Xuống (PgDown)",
        "PgUp": "Trang Lên (PgUp)",
        "OSKey": "Phím Hệ Điều Hành (OSKey)",
        "Slash": "Dấu Chéo (Slash)",
        "Minus": "Dấu Trừ (Minus)",
        "Plus": "Dấu Cộng (Plus)",
        "Down": "Xuống (Down)",
        "Up": "Lên (Up)",
        "MMB": "NCG (MMB)",
        "LMB": "NCT (LMB)",
        "RMB": "NCP (RMB)",
        "Pen": "Bút (Pen)"
    }

    KEYBOARD_TRANS_DIC_PURE = {
        "OSKey": "Phím Hệ Điều Hành (OSKey)",
        "WheelUp": "Lăn Bánh Xe về Trước (WheelUp)",
        "WheelDown": "Lăn Bánh Xe về Sau (WheelDown)",
        "Wheel": "Bánh Xe (Wheel)",
        "NumpadPlus": "Dấu Cộng (+) Bàn Số (NumpadPlus)",
        "NumpadMinus": "Dấu Trừ (-) Bàn Số (NumpadMinus)",
        "NumpadSlash": "Dấu Chéo (/) Bàn Số (NumpadSlash)",
        "NumpadDelete": "Dấu Xóa/Del Bàn Số (NumpadDelete)",
        "NumpadPeriod": "Dấu Chấm (.) Bàn Số (NumpadPeriod)",
        "NumpadAsterisk": "Dấu Sao (*) Bàn Số (NumpadAsterisk)",
        "Numpad0": "Số 0 Bàn Số (Numpad0)",
        "Numpad1": "Số 1 Bàn Số (Numpad1)",
        "Numpad2": "Số 2 Bàn Số (Numpad2)",
        "Numpad3": "Số 3 Bàn Số (Numpad3)",
        "Numpad4": "Số 4 Bàn Số (Numpad4)",
        "Numpad5": "Số 5 Bàn Số (Numpad5)",
        "Numpad6": "Số 6 Bàn Số (Numpad6)",
        "Numpad7": "Số 7 Bàn Số (Numpad7)",
        "Numpad8": "Số 8 Bàn Số (Numpad8)",
        "Numpad9": "Số 9 Bàn Số (Numpad9)",
        "Spacebar": "Dấu Cách (Spacebar)",
        "Down": "Xuống (Down)",
        "Up": "Lên (Up)",
        "Comma": "Dấu Phẩy (Comma)",
        "Minus": "Dấu Trừ (Minus)",
        "Plus": "Dấu Cộng (Plus)",
        "Left": "Trái (Left)",
        "=": "Dấu Bằng (=)",
        "Equals": "Dấu Bằng (=)",
        "Right": "Phải (Right)",
        "Backslash": "Dấu Chéo Ngược (Backslash)",
        "Slash": "Dấu Chéo (Slash)",
        "AccentGrave": "Dấu Huyền (AccentGrave)",
        "Delete": "Xóa (Delete)",
        "Period": "Dấu Chấm (Period)",
        "PageDown": "Trang Xuống (PageDown)",
        "PageUp": "Trang Lên (PageUp)",
        "PgDown": "Trang Xuống (PgDown)",
        "PgUp": "Trang Lên (PgUp)",
        "OSKey": "Phím Hệ Điều Hành (OSKey)",
        "MMB": "NCG (MMB)",
        "LMB": "NCT (LMB)",
        "RMB": "NCP (RMB)",
        "Pen": "Bút (Pen)"
    }

    numeric_prefix = 'hằng/lần thứ/bộ/bậc'
    numeric_postfix = 'mươi/lần/bậc'
    numeral_dict = {
        '@{1t}': 'ức',
        '@{1b}': 'tỉ',
        '@{1m}': 'triệu',
        '@{1k}': 'nghìn',
        '@{1h}': 'trăm',
        '@{10}': 'chục/mươi/mười',
        '@{0}': 'không/vô/mươi',
        '@{1}': 'một/nhất/đầu tiên',
        '@{2}': 'hai/nhì/nhị/phó/thứ/giây đồng hồ',
        '@{3}': 'ba/tam',
        '@{4}': 'bốn/tứ/tư',
        '@{5}': 'năm/lăm/nhăm/Ngũ',
        '@{6}': 'Sáu/Lục',
        '@{7}': 'Bảy/Thất',
        '@{8}': 'Số tám/bát',
        '@{9}': 'Chín/cửu',
    }

    numeric_trans = {
        'a|an': '@{1} con/cái/thằng',
        'zero|none|empty|nullary': '@{0}',
        'one|first|monuple|unary': '@{1}',
        'two|second|couple|binary': '@{2}',
        'three|third|triple|ternary': '@{3}',
        'four(th)?|quadruple|Quaternary': '@{4}',
        'five|fifth|quintuple|Quinary': '@{5}',
        'five|fifth|quintuple|Quinary': '@{5}',
        'six(th)?|sextuple|Senary': '@{6}',
        'seven(th)?|septuple|Septenary': '@{7}',
        'eight(th)?|octa|octal|octet|octuple|Octonary': '@{8}',
        'nine(th)?|nonuple|Novenary|nonary': '@{9}',
        'ten(th)?|decimal|decuple|Denary': '@{10}',
        'eleven(th)?|undecuple|hendecuple': 'Mười @{1}',
        'twelve(th)?|doudecuple': 'Mười @{2}',
        'thirteen(th)?|tredecuple': 'Mười @{3}',
        'fourteen(th)?|quattuordecuple': 'Mười @{4}',
        'fifteen(th)?|quindecuple': 'Mười @{5}',
        'sixteen(th)?|sexdecuple': 'Mười @{6}',
        'seventeen(th)?|septendecuple': 'Mười @{7}',
        'eighteen(th)?|octodecuple': 'Mười @{8}',
        'nineteen(th)?|novemdecuple': 'Mười @{9}',
        '(twent(y|ie(s|th))+?)|vigintuple': '@{2} @{10}',
        '(thirt(y|ie(s|th))+?)|trigintuple': '@{3} @{10}',
        '(fort(y|ie(s|th))+?)|quadragintuple': '@{4} @{10}',
        '(fift(y|ie(s|th))+?)|quinquagintuple': '@{5} @{10}',
        '(sixt(y|ie(s|th))+?)|sexagintuple': '@{6} @{10}',
        '(sevent(y|ie(s|th))+?)|septuagintuple': '@{7} @{10}',
        '(eight(y|ie(s|th))+?)|octogintuple': '@{8} @{10}',
        '(ninet(y|ie(s|th))+?)|nongentuple': '@{9} @{10}',
        '(hundred(s|th)?)|centuple': '@{1h}',
        '(thousand(s|th)?)|milluple': '@{1k}',
        'million(s|th)?': '@{1m}',
        'billion(s|th)?': '@{1t}',
        'trillion(s|th)?': '@{1t}',
    }
    words_should_avoid_forced_lower_list = [
        "biết đến",
        "bên trên",
        "bên trong",
        "bị động",
        "bỏ qua",
        "bổ sung",
        "bộc lộ ra",
        "chuẩn bị",
        "chuẩn bị",
        "chồng alpha lên",
        "chồng lên",
        "con cái",
        "cần làm",
        "dùng chung",
        "dưới cùng",
        "giãn ra",
        "hay hơn",
        "hiển thị trong",
        "hiện tại",
        "hoàn thành",
        "hình sự",
        "không có",
        "không được",
        "làm việc",
        "lên trang",
        "lên trên",
        "lên trên",
        "lõm vào",
        "lồi ra",
        "lớn lên",
        "mặt trên",
        "mở ra",
        "ngón chân cái",
        "ngón cái",
        "ngón tay cái",
        "người dùng",
        "nội trong",
        "phải làm",
        "quỹ đạo lên",
        "quỹ đạo xuống",
        "sang trọng",
        "sinh thành",
        "sự cố",
        "sự thật",
        "theo đuổi",
        "thu vào",
        "thành công",
        "thành phố",
        "thành đô",
        "thả trên",
        "trang lên",
        "trang xuống",
        "trong suốt",
        "trong suốt",
        "trong vắt",
        "trong vắt",
        "tràn ra",
        "tràn vãi ra",
        "trên cùng",
        "trở thành",
        "tất cả",
        "tuyệt đối",
        "tương đối",
        "phản đối",
        "từ chuyên môn",
        "từ trong",
        "vào ra",
        "vãi ra"
        "xuống trang",
        "đầu ra",
        "đầu vào",
        "đối xứng",
        ]
    phrases_should_be_lower_list = [
        'về một số',
        ]
    words_should_be_lower_list = [
        ':abbr:',
        ':class:',
        ':doc:',
        ':func:',
        ':guilabel:',
        ':kbd:',
        ':linenos:',
        ':math:',
        ':menuselection:',
        ':meth:',
        ':minute:',
        ':mod:',
        ':ref:',
        ':sup:',
        ':term:',
        'bị',
        'bởi',
        'cho',
        'chưa',
        'đối',
        'các',
        'cái',
        'còn',
        'có thể',
        'có',
        'nào',
        'đó',
        'cả',
        'của',
        'dùng',
        "đang",
        'hay',
        'hoặc',
        'khỏi',
        'là',
        'làm',
        'lên',
        'mà',
        'ngoài',
        'nhé',
        'như',
        'nhưng',
        'những',
        'nên',
        'qua',
        'ra',
        'sang',
        'sẽ',
        'sự',
        'theo',
        'thành',
        'thì'
        'trong',
        'trong',
        'trên',
        'tại',
        'tới',
        'từ',
        'và',
        'vào',
        'vì',
        'vậy',
        'về',
        'với',
        'xuống',
        'đã',
        'được',
        'đấy',
        'đến',
        'để',
        'ở',
    ]

    phrases_should_be_lower_txt = '|'.join(phrases_should_be_lower_list)
    ALL_PHRASES_SHOULD_BE_LOWER = re.compile(phrases_should_be_lower_txt, re.I)

    all_words_should_be_lower = '|'.join(words_should_be_lower_list)
    all_words_should_be_lower_pat_txt = r'^(%s)$' % (all_words_should_be_lower)
    all_words_should_be_lower_pat_in_first_txt = r'^(%s)\b' % (all_words_should_be_lower)
    ALL_WORDS_SHOULD_BE_LOWER = re.compile(all_words_should_be_lower_pat_txt, re.I)
    ALL_WORDS_SHOULD_BE_LOWER_IN_FIRST = re.compile(all_words_should_be_lower_pat_in_first_txt, re.I)
    SEP_WORDS = re.compile(r'\/| |\, |\. |; |\-|\:|\'|\(|\)?|\"|\/')
    SEP_CASE = re.compile(r'[A-Z]+|[a-z]+|[0-9]+')

    split_sent_seg_txt = r'\s?([\,\.\-\;]+(?<!((e\.g|i\.e|etc|fig)\.))\s)|([\(\)]|[{}])'
    PUNCTUATION_FINDER = re.compile(split_sent_seg_txt, flags=re.I)

    FULL_STOP_IN_SENTENCE = re.compile(r'\.\s+')

    split_sent_ignore_ga_tag_txt = r'\s?([\,\.\-\;]+(?<!((e\.g|i\.e|etc|fig)\.))\s)|([\(\)]|[{}])|(\:[\w]+\:)|(\:\s)'
    PUNCTUATION_WITHOUT_GA_TAG_FINDER = re.compile(split_sent_ignore_ga_tag_txt, flags=re.I)

    total_files = 1358
    file_count = 0
    PAGE_SIZE = 20 * 4096
    MAX_FUZZY_LIST = 100
    MAX_FUZZY_TEST_LENGTH = 0.5
    FUZZY_ACCEPTABLE_RATIO = 90
    FUZZY_MODERATE_ACCEPTABLE_RATIO = 85
    FUZZY_LOW_ACCEPTABLE_RATIO = 70
    FUZZY_VERY_LOW_ACCEPTABLE_RATIO = 45
    FUZZY_PERFECT_MATCH_PERCENT = 60

    APOSTROPHE_CHAR = "'"
    MAX_FUZZY_ACCEPTABLE_RATIO = 95
    FUZZY_RATIO_INCREMENT = 5
    AWESOME_COSSIM_FUZZY_ACCEPTABLE_RATIO = 50
    FUZZY_KEY_LENGTH_RATIO = 0.4

    # sentence structure patternssent
    MAX_SENT_STRUCT_CHOSEN = 20
    sent_struct_start_symb_txt = r'\$\{'
    SENT_STRUCT_START_SYMB = '${'

    SENT_STRUCT_START_SYMB_PAT = re.compile(sent_struct_start_symb_txt, flags=re.I)

    SENT_STRUCT_POSITION_PRIORITY_WEIGHT = 15

    regular_var = r'(\$\{([^\{\}]+)?\})'
    REGULAR_VAR_PAT = re.compile(regular_var)

    max_var_pat_txt = r'(MX(\d+))'
    MAX_VAR_PAT = re.compile(max_var_pat_txt)

    extra_mode = r'(\/([^\/]+))*'
    VAR_EXTRA_MODE = re.compile(extra_mode)

    word_boundary = r'[\/\n\s\:]'
    WORD_SPLIT = re.compile(word_boundary)

    sent_struct_pat_txt = r'%s' % (regular_var)
    # SENT_STRUCT_PAT = re.compile(r'((\${3})(\w+)?(\/\w+)*)')
    SENT_STRUCT_PAT = re.compile(sent_struct_pat_txt)

    ANY = re.compile(r'^.*$', re.I)
    EXCLUDE = re.compile(r'EX\([^\(\)]+\)', re.I)
    NOT_TRAILING = re.compile(r'NT\([^\(\)]+\)', re.I)
    NOT_LEADING = re.compile(r'NL\([^\(\)]+\)', re.I)
    EQUAL = re.compile(r'EQ\((.*)\)', re.I)
    EMBEDDED_WITH = re.compile(r'EMB\([^\(\)]+\)', re.I)
    LEADING_WITH = re.compile(r'LD\([^\(\)]+\)', re.I)
    TRAILING_WITH = re.compile(r'ED\([^\(\)]+\)', re.I)
    CLAUSED_PART = re.compile(r'\((.*)\)', re.I)

    emb_pat_char = r'\¡'
    emb_pat_part_txt = r'%s([^%s]+)%s' % (emb_pat_char, emb_pat_char, emb_pat_char)
    emb_pat_txt = r'^%s$' % (emb_pat_part_txt)
    PATTERN = re.compile(emb_pat_part_txt, re.I)
    PATTERN_PART = re.compile(emb_pat_part_txt)

    NUMBER_ONLY = re.compile(r'^nbr$', re.I)
    POSITION_PRIORITY = re.compile(r'^pp$', re.I)
    ORDERED_GROUP = re.compile(r'^\d+$', re.I)
    NO_PUNCTUATION = re.compile(r'^np$', re.I)
    MAX_UPTO = re.compile(r'^mx\d+?$', re.I)
    NO_CONJUNCTIVES = re.compile(r'^nc$', re.I)
    NO_FULL_STOP = re.compile(r'^nfs$', re.I)

    TRAN_REF_PATTERN = re.compile(r'\@\{([^{@}]+)?\}')

    python_format_txt = r'''
        \%
            (?:\(([\w]*)\))?
            (
                [-#0\ +]?(?:\*|[\d]+)?
                (?:\.(?:\*|[\d]+))?
                [hlL]?
            )
            ([diouxXeEfFgGcrs%])
    '''
    python_format_txt_absolute = r'^%s$' % (python_format_txt)
    PYTHON_FORMAT = re.compile(python_format_txt, flags=re.X)
    # PYTHON_FORMAT = re.compile(r'''
    #     \%
    #         (?:\(([\w]*)\))?
    #         (
    #             [-#0\ +]?(?:\*|[\d]+)?
    #             (?:\.(?:\*|[\d]+))?
    #             [hlL]?
    #         )
    #         ([diouxXeEfFgGcrs%])
    # ''', re.VERBOSE)

    PYTHON_FORMAT_ABS = re.compile(python_format_txt_absolute, flags=re.X)

    WEAK_TRANS_MARKER = "#-1#"
    debug_current_file_count = 0
    debug_max_file_count = 5
    debug_file = None

    # debug_file = 'addons/3d_view'
    # debug_file = 'animation/armatures/posing/bone_constraints/introduction' # e.g.
    # debug_file = 'animation/armatures/posing/bone_constraints/inverse_kinematics/introduction' # kbd WheelDown/Up
    # debug_file = "video_editing/sequencer/strips/effects/subtract"
    # debug_file = "video_editing/introduction"
    # debug_file = "about/contribute/index"
    # debug_file="interface/window_system/topbar"
    # debug_file = "advanced/app_templates"
    # debug_file = "modeling/empties"
    # debug_file = "animation/armatures/posing/editing"
    # debug_file = "index"
    # debug_file = "animation/constraints/relationship/shrinkwrap"
    # debug_file = "getting_started/about/community"
    # debug_file = "animation/actions"
    # debug_file = "video_editing/sequencer/strips/transitions/wipe" # :ref:`easings <editors-graph-fcurves-settings-easing>`
    # debug_file = "about/contribute/editing"
    # debug_file = "about/contribute/build/windows"
    # debug_file = "about/contribute/build/macos"
    # debug_file = "about/contribute/guides/maintenance_guide"
    # debug_file = "about/contribute/guides/markup_guide" # debugging :term: :abbr:, ``:kbd:`LMB```, ``*Mirror*``, ``:menuselection:`3D View --> Add --> Mesh --> Monkey```
    # debug_file = "about"
    # debug_file = "about/contribute/install/windows"
    # debug_file = "about/license" # (online) or URL (in print) to manual
    # debug_file = "addons/3d_view/3d_navigation" # debugging :menuselection:
    # debug_file = "addons/add_curve/index"
    # debug_file = "addons/add_curve/ivy_gen"
    # debug_file = "addons/import_export/anim_nuke_chan"
    # debug_file = "addons/node/node_wrangler"
    # debug_file = "addons/object/carver"
    # debug_file = "advanced/command_line/arguments" # trouble some file
    # debug_file = "advanced/command_line/introduction"
    # debug_file = "advanced/command_line/launch/macos"
    # debug_file = "animation/armatures/bones/editing/properties"
    # debug_file = "animation/constraints/relationship/shrinkwrap"
    # debug_file = "animation/constraints/tracking/damped_track"
    # debug_file = "compositing/types/color/color_balance"
    # debug_file = "compositing/types/color/hue_saturation"
    # debug_file = "editors/dope_sheet/introduction" # Pan the view vertically (values) or horizontally (time) with click and drag (:kbd:`MMB`).
    # debug_file = "editors/graph_editor/channels" # Box Select: (:kbd:`LMB` drag) or :kbd:`B` (:kbd:`LMB` drag)
    # debug_file = "editors/preferences/system"
    # debug_file = "editors/texture_node/types/converter/rgb_to_bw"
    # debug_file = "editors/timeline"
    # debug_file = "editors/uv/introduction"
    # debug_file = "files/media/image_formats"
    # debug_file = "getting_started/about/history"
    # debug_file = "grease_pencil/modes/draw/tool_settings/line"
    # debug_file = "interface/controls/nodes/editing"
    # debug_file = "manual/modeling/meshes/primitives"
    # debug_file = "modeling/meshes/editing/vertices"
    # debug_file = "modeling/meshes/structure"
    # debug_file = "modeling/surfaces/structure"
    # debug_file = "movie_clip/tracking/clip/properties/stabilization/introduction"
    # debug_file = "render/shader_nodes/textures/white_noise"
    # debug_file = "scene_layout/object/selecting"
    # debug_file = "scene_layout/scene/properties"
    # debug_file = "sculpt_paint/sculpting/hide_mask"
    # debug_file = "sculpt_paint/weight_paint/editing"
    # debug_file = "video_editing/sequencer/properties/strip"
    # debug_file = "video_editing/sequencer/strips/movie_image"

    leading = r'([\`\<]+)'
    ending = r'([\`\>]+)'
    word = r'([\w\d\#]+)'
    sep = r'([<>\\\/\-\_\.{}:]+)'
    sep_first = r'((%s(%s%s)+)+)' % (sep, word, sep)
    word_first = r'((%s(%s%s)+)+%s?)' % (word, sep, word, sep)
    word_first_with_leading_ending = r'(%s%s%s)' % (leading, word_first, ending)
    pat = r'%s|%s' % (word_first, sep_first)
    path_with_leading_and_ending = r'%s|%s' % (word_first_with_leading_ending, sep_first)
    pat_full = r'^(%s)$' % (pat)

    word = r'([\w\#]+)'
    ignore_words = r'((M[ris]+|Dr|etc|e.g)[\.])'
    url_leading = r'((http|https|file)\:\/\/)'
    URL_LEADING_PATTERN = re.compile(url_leading, re.I)

    path_sep = r'([\~\\\\////\\\/\_\-\.\:\*\?\=\{\}\|]{1,2})'
    leading_hyphens = r'(^[-]+)'
    ref_tag = r'(^:%s:$)' % (word)
    single_hyphen = r'(^%s[-:*_\/]%s$)' % (word, word)
    number_format = r'(\d+[.]\d+)'
    hour_format = r'(%s:%s(:%s)?([.]%s)?)' % (word, word, word, word)
    whatever = r'(%s?)[*]{1}(%s?)' % (word, word)
    file_extension = r'([.]%s{2,5})$' % (word)
    return_linefeed = r'^(\\[nr])$'
    bold_word = r'^(\*%s\*)$' % (word)
    not_allowed = r'(?!(%s|%s|%s|%s|%s|%s|%s|%s))' % (
    ignore_words, bold_word, leading_hyphens, single_hyphen, ref_tag, hour_format, number_format, return_linefeed)
    path = r'(%s|%s)?((%s(%s)?%s)+)+' % (word, path_sep, path_sep, path_sep, word)
    variable = r'[\w_-]+'
    api_path = r'((%s\.%s)+)+' % (variable, variable)
    blender_api = r'^(blender_api\:%s)$' % (api_path)

    extension_0001 = r'(%s\.%s)' % (word, word)
    extension_0002 = r'(%s\.%s)' % (whatever, word)
    extension_0003 = r'(%s\.%s)' % (word, whatever)
    extension_0004 = r'(%s\.%s)' % (whatever, whatever)

    ending_extension = r'(%s|%s|%s|%s)' \
                       % ( \
                           extension_0001, \
                           extension_0002, \
                           extension_0003, \
                           extension_0004,
                       )
    path_def = r'^(%s|%s)%s?(%s)?$' % (path, url_leading, path_sep, ending_extension)
    # path_def = r'^%s(%s)%s?$' % (not_allowed, path, path_sep)
    path_pattern = r'%s(%s|%s|%s)' % (not_allowed, path_def, file_extension, blender_api)
    PATH_CHECKER = re.compile(path_pattern, flags=re.I)

    # meta_char_list = "[].^$*+?{}()\|"
    METACHAR_PATTERN = re.compile(r'[\[\]\.\^\$\*\+\?\{\}\(\)\\\|]', re.M)
    PREFIX_END = r'[^0-9@#.,]'
    NUMBER_TOKEN = r'[0-9@#.,E+]'

    PREFIX_PATTERN = r"(?P<prefix>(?:'[^']*'|%s)*)" % PREFIX_END
    NUMBER_PATTERN = r"(?P<number>%s*)" % NUMBER_TOKEN
    SUFFIX_PATTERN = r"(?P<suffix>.*)"

    NUMBER_RE = re.compile(r"%s%s%s" % (PREFIX_PATTERN, NUMBER_PATTERN,
                                        SUFFIX_PATTERN))
    WHITESPACE = re.compile('[\s]+')
    EMAIL_ADDRESS = re.compile(r"^\s*.+@[^\.].*\.[a-z]{2,}$")  # start to end
    DOC_LINK = re.compile(r'^(\/\w+)+$')

    WORD_SEPARATION = re.compile('('
                                 r'\s+|'  # any whitespace
                                 r'[^\s\w]*\w+[a-zA-Z]-(?=\w+[a-zA-Z])|'  # hyphenated words
                                 r'(?<=[\w\!\"\'\&\.\,\?])-{2,}(?=\w)'  # em-dash
                                 ')')

    REF_PATH = re.compile(r'^\w+([\-\.]\w+){1,}$')
    DOC_PATH = re.compile(r'^(\/\w+)+$')

    KBD = 'kbd'
    MNU = 'menuselection'
    DOC = 'doc'
    ABBREV = 'abbr'
    STD_REF = 'std-ref'
    X_REF = 'xref'
    REF_URI = 'refuri'
    GUI_LAB = 'guilabel'
    TAG_ABBR = 'abbreviation'
    TAG_NAME = 'tagname'
    CLASS = 'classes'

    var = r'[A-Za-z][\w\_\-\.]*'
    param_txt = r'[\w\d\.\_]+'
    param = r'(%s)(\,\s+?(%s))*' % (param_txt, param_txt)
    funct_pat_txt = r'(?<!\\)(%s)\((%s?\))(?<!\w\(s\))(?<!\w\(ren\))' % (var, param)
    funct_only_pat_txt = r'^(%s)$' % (funct_pat_txt)
    FUNCTION = re.compile(funct_pat_txt)
    FUNCTION_ABS = FUNCTION

    FORWARD_SLASH = re.compile(r'[\w\s]?([\/]+)[\w\s]?')
    UPPER_CASE_WORDS = re.compile(r'(?=(\s+|^))([A-Z]+)(?=(\s+|$))', re.UNICODE)

    email = r'(<)?(\w+@\w+(?:\.\w+)+)(?(1)>|$)'
    sentence_elements = r'([^\.\,\:\!]+)'
    not_follow_by_a_space = r'(?!\s)'
    follow_by_a_space_or_end = r'(?:(\s|$))'
    not_precede_by_a_space = r'(?<![\s\d])'
    # setence_break_pat_txt = r'%s%s%s' % (not_precede_by_a_space, sentence_elements, follow_by_a_space_or_end)
    setence_break_pat_txt = r'%s%s' % (sentence_elements, follow_by_a_space_or_end)
    COMMON_SENTENCE_BREAKS = re.compile(setence_break_pat_txt)

    TRIMMABLE_ENDING = re.compile(r'([\s\.\,\:\!]+)$')
    TRIMMABLE_BEGINNING = re.compile(r'^([\s\.\,]+)')
    TRAILING_WITH_PUNCT = re.compile(r'[\s\.\,\:\!\'\%\$\"\\\)\}\|\]\*\?\>\`\-\+\/\#\&]$')
    HEADING_WITH_PUNCT = re.compile(r'^[\s\.\,\:\!\'\%\$\"\\\(\{\|\[\*\?\>\`\-\+\/\#\&]')

    TRAILING_WITH_PUNCT_MULTI = re.compile(r'[\s\.\,\:\!\'\%\$\"\\\*\?\-\+\/\#\&]+$')
    HEADING_WITH_PUNCT_MULTI = re.compile(r'^[\s\.\,\:\!\'\%\$\"\\\*\?\-\+\/\#\&]+')

    REMOVABLE_SYMB_FULLSET_FRONT = re.compile(r'^[\s\:\!\'$\"\\\(\{\|\[\*\?\;\<\`\-\+\/\#\&]+')
    REMOVABLE_SYMB_FULLSET_BACK = re.compile(r'[\s\:\!\'$\"\\\)\}\|\]\*\?\>\;\`\-\+\/\#\&\,\.]+$')

    RETAIN_FIRST_CHAR = re.compile(r'^[\*\'\"]+')
    RETAIN_LAST_CHAR = re.compile(r'[\*\'\"]+$')

    LEADING_WITH_SYMBOL = re.compile(r'^[\(\[]+')
    TRAILING_WITH_SYMBOL = re.compile(r'[\)\]]+$')

    ABBR_SEARCH = re.compile(r':abbr:\`([^\`]+(?<=\)))\`')
    ABBR_SPLIT = re.compile(r':abbr:')
    ABBR_TERM = re.compile(r'\)\`')

    GA_PATTERN_PARSER = re.compile(r':[\w]+:[\`]+([^\`]+)?[\`]+')
    ABBREV_PATTERN_PARSER = re.compile(r':abbr:[\`]+(.*?)[\`]+', flags=re.I)
    GUI_LABEL_PARSER = re.compile(r':guilabel:\`+([^\`]+)\`+', flags=re.I)

    ABBREV_PATTERN_PARSER_COR = re.compile(r'[^:]abbr:[\`]+(.*?)[\`]+', flags=re.I)
    ABBREV_PATTERN_PARSER_FULL = re.compile(r'^:abbr:[\`]+([^\`]+)[\`]+$', flags=re.I)
    ABBREV_CONTENT_PARSER = re.compile(r'([^\(\)]+)\s\(([^\)]+)\)')

    ABBREV_FRONT = re.compile(r':abbr:[\`]+\(')
    GA_BACK = re.compile(r'\)[\`]+')

    punctuals = r'([\\\/\.\,\:\;\!\?\"\*\'\`]+)'
    basic_punctuals = r'([\.\,\;\!\:]+(\s+|$))'

    PUNCTUALS = re.compile(punctuals)
    BASIC_PUNCTUALS = re.compile(basic_punctuals)

    begin_punctuals = r'^%s' % (punctuals)
    end_punctuals = r'%s$' % (punctuals)
    begin_basic_punctuals = r'^%s' % (basic_punctuals)
    end_basic_punctuals = r'%s$' % (basic_punctuals)

    single = r'{1}'
    punctual_single = r'(%s%s)' % (punctuals, single)
    end_punctual_single = r'%s$' % (punctual_single)
    begin_punctual_single = r'^%s' % (punctual_single)

    simple_basic_punctuals = r'([\.\,\!\;]+)'
    end_punctual_in_mid_sentence = r'^%s\s?$' % (simple_basic_punctuals)
    BEGIN_AND_END_BASIC_PUNCTUAL_IN_MID_SENT = re.compile(end_punctual_in_mid_sentence)

    BEGIN_PUNCTUAL_MULTI = re.compile(begin_punctuals)
    BEGIN_PUNCTUAL_SINGLE = re.compile(begin_punctual_single)
    ENDS_PUNCTUAL_MULTI = re.compile(end_punctuals)
    ENDS_PUNCTUAL_SINGLE = re.compile(end_punctual_single)

    BEGIN_BASIC_PUNCTUAL = re.compile(begin_basic_punctuals)
    END_BASIC_PUNCTUAL = re.compile(end_basic_punctuals)

    WORD_ONLY = re.compile(r'\b([\w\.\/\+\-\_\<\>]+)\b')
    REF_SEP = ' -- '
    NON_WORD_ONLY = re.compile(r'^([\W]+)$')
    NON_WORD = re.compile(r'([\W]+)')
    NON_WORD_ENDING = re.compile(r'([\W]+)$')
    NON_WORD_STARTING = re.compile(r'^([\W]+)')

    SYMB_ENDING = re.compile(r'([\W]+)$')
    SYMB_STARTING = re.compile(r'^([\W]+)')

    SYMB_ENDING_MULTI = re.compile(r'([\W]{,2})$')
    SYMB_STARTING_MULTI = re.compile(r'^([\W]{,2})')

    TRANSLATABLE_CHARACTERS = re.compile(r'[a-zA-Z]+')

    sent_no_ref_txt = r'\s?((?!\s)[%s\s]+)(?<!\s)\s?' % (vn_uppercase_letters_and_digits)
    SENTENCE_NO_REF = re.compile(sent_no_ref_txt, flags=(re.UNICODE | re.IGNORECASE))

    attrib_pat_txt = r'(%s)\:(%s)' % (var, var)
    attrib_pat_abs_txt = r'^(%s)$' % (attrib_pat_txt)
    ATTRIB_REF = re.compile(attrib_pat_abs_txt)
    ATTRIB_REF_ABS = ATTRIB_REF

    GA_REF_PART = re.compile(r':[\w]+:', re.I)
    # GA_REF = re.compile(r'[\`]*(:[^\:]+:)*[\`]+(?![\s]+)([^\`]+)(?<!([\s\:]))[\`]+[\_]*')
    # GA_REF = re.compile(r'[\`]*(:[^\:]+:)*[\`]+([^\`]+)[\`]+[\_]*')
    NOT_SPACE = r'(?<!\s)'
    GA_SYMB = r'\`'
    ga_value_ref_pat_txt = r'[%s]+(%s+[^%s]+%s)[%s]+' % (GA_SYMB, NOT_SPACE, GA_SYMB, NOT_SPACE, GA_SYMB)
    ga_ref_pat_txt = r'[\`]*(:[^\:]+:)*%s[\_]*' % (ga_value_ref_pat_txt)
    ga_generic_only = r'^(%s)$' % (ga_ref_pat_txt)
    GA_GENERIC = re.compile(ga_ref_pat_txt)
    GA_GENERIC_ONLY = re.compile(ga_generic_only)

    GA_DOUBLE = re.compile(r'\`{2}[^\`]+\S\`{2}')
    GA_INTERNAL_LINK = re.compile(r'(?!:)\`[^\`]+(?:\S)\`\_(?!\_)')
    GA_EXTERNAL_LINK = re.compile(r'\`([^`<>]+)\s\<[^\<\>]+\>\`\_{2}')

    GA_REF = re.compile(ga_ref_pat_txt)
    GA_REF_ABS = re.compile(r'^[\`]*(:[^\:]+:)*[\`]+(?![\s]+)([^\`]+?)(?<!([\s\:]))[\`]+[\_]*(?:\W|$)?$')

    # ARCH_BRAKET = re.compile(r'[\(]+(?![\s\.\,]+)([^\(\)]+)[\)]+(?<!([\s\.\,]))')
    OSL_ATTRIB = re.compile(r'[\`]?(\w+:\w+)[\`]?')
    COLON_CHAR = re.compile(r'\:')
    COLON_CHAR_START = re.compile(r'^[\:]')
    COLON_CHAR_END = re.compile(r'[\:]$')

    EXPLANATION_PPROMPT = re.compile(r'(\:\s)')

    # this (something ... ) can have other links inside of it as well as others
    # the greedy but more accurate is r'[\(]+(.*)?[\)]+'
    # ARCH_BRAKET_SINGLE_PARTS = re.compile(r'[\)]+([^\(]+)?[\(]+')
    pairable_brackets_txt = r'[\[\]\(\)\<\>]'
    PAIRABLE_BRACKETS = re.compile(pairable_brackets_txt)
    exchange_bracket_txt = r'[\(\[\]\)]'
    EXCHANGE_BRAKETS = re.compile(exchange_bracket_txt)

    angle_bracket_single_txt = r'\<([^\<\>]+)[^\/]\>'
    arch_bracket_single_txt = r'\(([^\)\(]+?)\)'
    arch_bracket_single_full = r'\b%s|%s\b' % (arch_bracket_single_txt, angle_bracket_single_txt)
    arch_bracket_single_absolute = r'^%s(?:\W|$)?$' % (arch_bracket_single_txt)
    ARCH_BRAKET_SINGLE_FULL = re.compile(arch_bracket_single_full)
    ARCH_BRAKET_SINGLE_ABS = re.compile(arch_bracket_single_absolute)
    ARCH_BRAKET_SINGLE = re.compile(arch_bracket_single_txt)

    ARCH_BRAKET_MULTI_SIMPLE = re.compile(r'(?<!\w)\(((?=\S).+?(?<=\S))\)(?!\w)')

    ##### new ref
    GA_DOUBLE_EMBEDDED_GA = re.compile(r'\`{2}(:\w+:\`((?!\s)[^\`]+?)\`)\`{2}', re.I)
    GA_INTERNAL_LINK = re.compile(r'(?!:)\`((?!\s)[^`]+?(?<=\S))\`[_]+')

    GA_DOUBLE = re.compile(r'\`{2}([^\`]+?)\`{2}(?<!\(\s)')
    SINGLE_QUOTE = re.compile(r'(?<!\w)\'+([^\'\`]+)\'+')
    DOUBLE_QUOTE = re.compile(r'\"+((?![\s\,\`])[^"]+)\"+(?<!\(\s)')
    # ast_quote_txt = r'[\*]{1,2}((?:\S)[^\*]+(?:\S))[\*]{1,2}'## r'(?<!\\")(")(\S[^\"]+\S)(")'
    # ast_quote_txt = r'[\*]{2}([^\*]+)[\*]{2}'  ## r'(?<!\\")(")(\S[^\"]+\S)(")'

    ast_quote_txt_dbl = r'(?<!\*)\*{2}((?:\S).+?)\*{2}(?!=\*)'

    ast_generic_txt = r'[\*]+((?=\w)[^\*].+?)[\*]+'
    AST_GENERIC_QUOTES = re.compile(ast_generic_txt)

    ast_generic = r'[\*]+((?=\w)[^\*].+?)[\*]+'
    ast_quote_txt_sng = r'(?<!\*)[\*]([^\*]+)[\*](?!\*)'
    # ast_quote_txt_sng_loose = r'(?<=\s)[\*]((?![\s\*])(.+?))[\*]+(?!=\*)'
    ast_quote_txt_sng_loose = r'(?<!\*)[\*]([\w\*\s]+?)[\*]+(?!\*)'
    ast_quote_single_txt = r'(%s)' % (ast_quote_txt_sng)
    DBL_AST_QUOTES = re.compile(ast_quote_txt_dbl)
    # ast_quote_txt = ast_quote_txt_sng
    SNG_AST_QUOTE = re.compile(ast_quote_single_txt)   # r'[\*]((?!\s).+?(?<!\s))[\*](?=(\s|$))'
    REF_GENERIC = re.compile(r':\w+:(?!:)[\`]((?!\s)[^\`]+)[\`]', re.I)
    REF_GENERIC_WITH_LINK = re.compile(r':\w+:(?!:)[\`]((?!\s)[^\`\<\>]+(\s<([^\`\<\>]+)>))[\`]', re.I)
    # GA_GENERIC_DOUBLE = re.compile(r'[\`]{2}((?:\S)[^\`]?)(?:\S)[\`]{2}(?=(\s|$))', re.I)
    GA_GENERIC_DOUBLE = re.compile(r'[\`]{2,}(.+?)[\`]{2,}')
    GA_GENERIC_SINGLE = re.compile(r'(?!=\:)\`((?=\S)[^\`]+(?=\S))\`(\_+)?(?:(\s|$))', re.I)
    REF_GENERIC_STARTER = re.compile(r':\w+:', re.I)

    SPACES = re.compile(r'\s+')

    # ignoring math,
    REF_CONTENT_WITH_LINK_SPLITTER = re.compile(r'([^\<\>]+)\s(\<[^\<\>]+\>)')
    # for term, doc, ref, ga_single(`txt`__) -- ignore if no link, format :...:`vn_txt (en_txt) <link>`(__)

    MENU_SELECTION = re.compile(r':menuselection:\`([^`]+)(\s[\-]{2}>\s([^`]+))*\`')
    MENU_TXT_SPLITTER = re.compile(r'\s[\-]{2}>\s')

    ABBR_SPLITER = re.compile(r':abbr:\`([^`]+)\s\(([^<>]+)\)\`')
    GUI_LABEL_SPLITER = re.compile(r':guilabel:\`([^`]+)\`')
    GA_DOUBLE_SPLITTER = re.compile(r'[\`]{2}([^`]+)[\`]{2}')
    GA_SYMB_LEADING = re.compile(r'(?<!\:)\`+((?:[<\-#]+)([^`]+(?<!\:)))(?<![>\-#])\`+')
    GA_ONLY = re.compile(r'(?<![\:\_])\`+((?=[^\s\_])[^\`<>:]+(?<=[\S\(]))\`+(?!=\_)')

    ##### new ref

    ast_quote_txt = r'(?!:\s)\*([^\*]+)\*(?<!\s)'
    ast_quote_txt_absolute = r'^%s$' % (ast_quote_txt)
    SNG_QUOTE = re.compile(ast_quote_txt)
    SNG_QUOTE_ABS = re.compile(ast_quote_txt_absolute)

    dbl_quote_txt = r'"+([^"]+)"+'
    dbl_quote_txt_abs = r'^%s$' % (dbl_quote_txt)
    DBL_QUOTE = re.compile(dbl_quote_txt)
    DBL_QUOTE_ABS = re.compile(dbl_quote_txt_abs)

    # SNG_QUOTE = re.compile(r'[\']+([^\']+)[\']+(?!([\w]))')
    # single_quote_txt = r"'(?!\s)((?!([sdt]|ll|ve|nt))[^\']+)'"
    single_quote_txt = r"(?<=[\\\s\b])'+([^\']+)'+"
    single_quote_txt_absolute = r'^%s$' % (single_quote_txt)
    SNG_QUOTE = re.compile(single_quote_txt)
    SNG_QUOTE_ABS = re.compile(single_quote_txt_absolute)
    BLANK_QUOTE_MARK = '§'
    DBL_QUOTE_SLASH = re.compile(r'\\[\"]+(?![\s\.\,\`]+)([^\\\"]+)\\[\"]+(?<!([\s\.\,]))')
    WORD_WITHOUT_QUOTE = re.compile(r'^[\'\"\*]*([^\'\"\*]+)[\'\"\*]*$')
    blank_quote_txt = r'(?<!\w)(\%s)([^\%s]+)(?:\b)(\%s)' % (BLANK_QUOTE_MARK, BLANK_QUOTE_MARK, BLANK_QUOTE_MARK)
    blank_quote_txt_abs = r'^%s(?:\W|$)?$' % (blank_quote_txt)
    BLANK_QUOTE = re.compile(blank_quote_txt)
    BLANK_QUOTE_ABS = re.compile(blank_quote_txt_abs)

    LINK_WITH_URI = re.compile(r'([^\<\>\(\)]+[\w]+)[\s]+[\<\(]+([^\<\>\(\)]+)[\>\)]+[\_]*')
    MENU_PART = re.compile(
        r'([\s]?[-]{2}[\>]?[\s]+)(?![\s\-])([^\<\>]+)(?<!([\s\-]))')  # working but with no empty entries
    MENU_PART_1 = re.compile(r'(?!\s)([^\->])+(?<!\s)')
    MENU_SEP = re.compile(r'\s?([\-]+\>)\s?')

    ABBR = re.compile(r':abbr:\`([^\`\(\)]+)\s\(([^\`\(\)]+)\)\`', re.I)
    ABBREV_TEXT_REVERSE = re.compile(r'(?!\s)([^\(\)]+)(?<!\s)')
    REF_TEXT_REVERSE = re.compile(r'([^\`]+)\s\-\-\s([^\<]+)(?<![\s])')
    REF_PART = re.compile(r'([<(][^<>()]+[>)])')
    END_WITH_REF = re.compile(r'([<][^<>]+[>])$')
    HYPHEN_REF_LINK = re.compile(r'^(\w+)(\-\w+){2,}$')
    LINK_ALL = re.compile(r'^([/][\w_]+)+$')
    MENU_TEXT_REVERSE = re.compile(r'(?!\s)([^\(\)\-\>]+)(?<!\s)')

    path_sep = r'[\\\/\-\_\.]'
    PATH_SEP = re.compile(path_sep)
    NON_PATH_SEP = re.compile(r'^[^\\\/\-\_\.]+$')

    WORD_ONLY_FIND = re.compile(r'\b[\w\-\_\']+\b')
    NON_WORD_FIND = re.compile(r'\W+')
    WORD_START_REMAIN = re.compile(r'^\w+', flags=re.I)
    WORD_END_REMAIN = re.compile(r'\w+$', flags=re.I)

    ENDS_WITH_EXTENSION = re.compile(r'\.([\w]{2,5})$')
    MENU_KEYBOARD = re.compile(r':(kbd|menuselection):')
    MENU_TYPE = re.compile(r'^([\`]*:menuselection:[\`]+([^\`]+)[\`]+)$')
    MENU_EX_PART = re.compile(r'(\s?[\-]{2}\>\s?)')

    KEYBOARD_TYPE = re.compile(r'^([\`]*:kbd:[\`]+([^\`]+)[\`]+)$')
    KEYBOARD_SEP = re.compile(r'[^\-\s]+')
    SPECIAL_TERM = re.compile(r'^[\`\*\"\'\(]+(.*)[\`\*\"\'\)]+$')
    ALPHA_NUMERICAL = re.compile(r'[\w]+')
    EXCLUDE_GA = re.compile(r'^[\`\'\"\*\(]+?([^\`\'\"\*\(\)]+)[\`\'\"\*\)]+?$')
    OPTION_FLAG = re.compile(r'^[\-]{2}([^\`]+)')
    REF_FILLER_CHAR = '¢'
    ref_filler_char_pat_txt = r'[%s]+' % (REF_FILLER_CHAR)
    REF_FILLER_PAT = re.compile(ref_filler_char_pat_txt)

    IGNORABLE_OPTION_FLAGS = re.compile(r'^(([\-]+\w+)(\s[\+\-]?\d+)?\s?)+$')

    REF_MASK_CHAR = '#'
    REF_MASK_STR = f'{REF_MASK_CHAR * 2}'
    FILLER_CHAR = '¶'
    filler_char_pattern_str = r'[%s]+' % FILLER_CHAR
    FILLER_CHAR_PATTERN = re.compile(filler_char_pattern_str)
    filler_all_pattern_str = r'^(%s)$' % filler_char_pattern_str
    FILLER_ALL_PATTERN = re.compile(filler_all_pattern_str)

    filler_char_and_space_pattern_str = r'[%s\s]+' % (FILLER_CHAR)
    FILLER_CHAR_INVERT = re.compile(filler_char_and_space_pattern_str)

    filler_parts = r'\s?([%s]+)\s?' % (FILLER_CHAR)
    FILLER_PARTS = re.compile(filler_parts)

    filler_char_and_space_pattern_str = r'^[\s%s]+$' % FILLER_CHAR
    FILLER_CHAR_AND_SPACE_ONLY_PATTERN = re.compile(filler_char_and_space_pattern_str)

    filler_char_all_pattern_str = r'^[%s\s\W]+$' % FILLER_CHAR
    FILLER_CHAR_ALL_PATTERN = re.compile(filler_char_all_pattern_str)

    not_filler_char_txt = r'[^%s]+' % (FILLER_CHAR)
    not_filler_char_start_txt = r'^[^%s]+' % (FILLER_CHAR)
    not_filler_char_end_txt = r'[^%s]+$' % (FILLER_CHAR)

    NOT_FILLER_CHARS = re.compile(not_filler_char_txt)
    NOT_FILLER_CHARS_START = re.compile(not_filler_char_start_txt)
    NOT_FILLER_CHARS_END = re.compile(not_filler_char_end_txt)

    NEGATE_FILLER = r"[^\\" + FILLER_CHAR + r"]+"
    NEGATE_FIND_WORD = re.compile(NEGATE_FILLER)
    ABBR_TEXT = re.compile(r'\(([^\)]+)\)')
    ABBR_TEXT_ALL = re.compile(r':abbr:\`([^\(]+[^\(\)])\s\(([^\(\)]+)\)')
    REF_WITH_LINK = re.compile(r'([^\<\>\(\)]+)\s+?([\<\(]([^\<\>\(\)]+)[\)\>])?')
    REF_WITH_HTML_LINK = re.compile(r'([^\<\>]+)\s+?(\<([^\<\>]+)\>)?')

    IS_A_PURE_LINK = re.compile(r'^(?P<sep>[\/\-\\\.])?[^.*(?P=sep)]+(.*(?P=sep).*[^(?P=sep)]+){2,}$')

    REF_LINK = re.compile(r'[\s]?[\<]([^\<\>]+)[\>][\s]?')
    TERM_LINK = re.compile(r'([^\`]+)\<?[^\`]+\>?')
    PURE_PATH = re.compile(r'^(([\/\\][\w]+)([\/\\][\w]+)*)+[\/\\]?$')
    PURE_REF = re.compile(r'^([\w]+([\-][\w]+)+)+$')
    API_REF = re.compile(r'^blender_api:.*$')

    SPACE_WORD_SEP = re.compile(r'[\S]+')
    WORD_ONLY = re.compile(r'[\w]+')
    ACCEPTABLE_WORD = re.compile(r'[\w\-]+([\'](t|ve|re|m|s))?')
    QUOTED_MSG_PATTERN = re.compile(r'((?<![\\])[\'"])((?:.)*.?)')
    BLENDER_DOCS = os.path.join(os.environ['HOME'], 'blender_docs')

    titled_word_txt = r'\b([A-Z][a-z]+\b)+'
    TITLED_WORDS = re.compile(titled_word_txt)

    # WORD_SEP = re.compile(r'[\s\;\:\.\,\/\!\-\dd\<\>\(\)\`\*\"\|\']')
    CHARACTERS = re.compile(r'[\w\-]+', re.UNICODE)
    WORD_SEP = re.compile(r'[^\W]+')
    REMOVALBLE_SYMBOLS = re.compile(r'^[^\w\%\º\?]+$', re.UNICODE)
    SYMBOLS_ONLY = re.compile(r'^[\W\s]+$')
    NON_SPACE_SYMBOLS = re.compile(r'[^\s\w\d]+')
    SYMBOLS = re.compile(r'[\W]+')

    NOT_CHARS_AND_SPACES = re.compile(r'[^\s\w]+')

    NON_ALPHA_NUMERIC = re.compile(r'[^A-Za-z]')
    UNDER_SCORE = re.compile(r'[\_]+')

    NON_SYMBOL_AND_SPACE = re.compile(r'[^\w\s]+')

    SPACES = re.compile(r'\s+')

    START_SPACES = re.compile(r'^\s+')
    END_SPACES = re.compile(r'\s+$')

    common_multi_word_connectors = r'[\s\-\,\/]'
    COMMON_WORD_SEPS = re.compile(common_multi_word_connectors)

    NOT_SYMBOLS = re.compile(r'[\w]+')
    SPACE_SEP_WORD = re.compile(r'[^\s]+')
    SPACE_SEP_WORD_AND_FSLASH = re.compile(r'[^\s\/]+')

    THE_WORD = re.compile(r'\bthe\b\s?', re.I)
    POSSESSIVE_APOS = re.compile(r'(\'s)\b')

    MULTI_SPACES = re.compile(r'[\s]{2,}')
    HYPHEN = re.compile(r'[\-]')
    SPACE_SEP = re.compile(r'\s+')

    SPACE_GA_SEP = re.compile(r'[\`\(\)\!\,\.\'\*\&\s\=\/\[\]\|\-]+|\:\w+\:|\:\s|\%[sdxf]\d+?]')
    NON_SPACE_WORDS = re.compile(r'([\S]+)')

    full_stop_in_middle = r'([\S][\.]\s[\S])'
    comma_in_middle = r'([\S]\,\s[\S])'
    punct_in_between_txt = r'(%s|%s)' % (full_stop_in_middle, comma_in_middle)
    PUNCT_IN_BETWEEN = re.compile(punct_in_between_txt)
    FULLSTOP_IN_BETWEEN = re.compile(full_stop_in_middle)

    SIMPLE_PUNCTUATIONS = re.compile(r'[\.\,\:\!]+(\s|$)|^\)\s+|^\s\-\-\s|\s+\($|^\s+|\s+$')
    SPACE_AT_MARGINS = re.compile(r'^\s+|\s+$')

    ending_punct = r'(\w[\,\.!]+$)'
    ENDING_WITH_PUNCT = re.compile(ending_punct)

    basic_conjunctions = r'(for|to|is|are|was|were|and|nor|in|by|out|that|then|above|below|up|down|but|or|yet|so|etc(\W+)?)'

    basic_conjunctions_pat_txt = r'(\s|^)%s(\s|$)' % (basic_conjunctions)
    BASIC_CONJUNCTS = re.compile(basic_conjunctions_pat_txt)

    basic_conjunctions_only_pat_txt = r'^%s$' % (basic_conjunctions)
    BASIC_CONJUNCTS_ONLY = re.compile(basic_conjunctions_only_pat_txt)

    MAXWORD_UPTO_PAT = re.compile(r'^mx(\d+)$')

    START_WORD = '^'
    END_WORD = '$'
    BOTH_START_AND_END = '^$'

    START_WORD_SYMBOLS = re.compile(r'^[\W\_]+')
    END_WORD_SYMBOLS = re.compile(r'[\W\_]+$')
    ALL_WORD_SYMBOLS = re.compile(r'^[\W\_]+$')

    left_set_brackets = ['(', '[', '<', '{']
    right_set_brackets = [')', ']', '>', '}']

    LEFT_SET_BRACKET = re.compile(r'[\(\[\<\{]+')
    RIGHT_SET_BRACKET = re.compile(r'[\)\]\>\}]+')
    PAIR_BRACKETS = re.compile(r'[\(\)]')


    EN_DUP_ENDING = re.compile(r'[aeiou]\w{1}$')

    FILE_EXTENSION = re.compile(r'^[\.//]\w{2,}$')
    FILE_NAME_WITH_EXTENSION = re.compile(r'(?:[^\+\-\=\s])[\w\-\_\*]+\.\w+$')
    WORD_SPLITTER = None
    # nlp = spacy.load('en_core_web_sm')

    BRACKET_OR_QUOTE_REF = re.compile(r'(_QUOTE|_BRACKET)')

    verb_with_ending_y = [
        'aby', 'bay', 'buy', 'cry', 'dry', 'fly', 'fry', 'guy', 'hay',
        'joy', 'key', 'lay', 'pay', 'ply', 'pry', 'ray', 'say', 'shy',
        'sky', 'spy', 'toy', 'try', 'ally', 'baby', 'body', 'bray', 'buoy',
        'bury', 'busy', 'cloy', 'copy', 'defy', 'deny', 'eddy', 'envy',
        'espy', 'flay', 'fray', 'gray', 'grey', 'levy', 'obey', 'okay',
        'pity', 'play', 'pray', 'prey', 'rely', 'scry', 'slay',
        'spay', 'stay', 'sway', 'tidy', 'vary', 'allay', 'alloy', 'annoy',
        'apply', 'array', 'assay', 'bandy', 'belay', 'belly', 'berry',
        'bogey', 'bully', 'caddy', 'candy', 'carry', 'chevy', 'chivy',
        'colly', 'curry', 'dally', 'decay', 'decoy', 'decry', 'deify', 'delay',
        'dirty', 'dizzy', 'dummy', 'edify', 'empty', 'enjoy', 'ensky', 'epoxy',
        'essay', 'fancy', 'ferry', 'foray', 'glory', 'harry', 'honey', 'hurry',
        'imply', 'inlay', 'jelly', 'jimmy', 'jolly', 'lobby', 'marry', 'mosey',
        'muddy', 'palsy', 'parry', 'party', 'putty', 'query', 'rally', 'ready',
        'reify', 'relay', 'repay', 'reply', 'retry', 'savvy', 'splay', 'spray',
        'stray', 'study', 'stymy', 'sully', 'tally', 'tarry', 'toady', 'unify',
        'unsay', 'weary', 'worry', 'aerify', 'argufy', 'basify', 'benday',
        'betray', 'bewray', 'bloody', 'canopy', 'chivvy', 'citify', 'codify',
        'comply', 'convey', 'convoy', 'curtsy', 'defray', 'deploy', 'descry',
        'dismay', 'embody', 'employ', 'flurry', 'gasify', 'jockey', 'minify',
        'mislay', 'modify', 'monkey', 'motley', 'mutiny', 'nazify', 'notify',
        'occupy', 'ossify', 'outcry', 'pacify', 'parlay', 'parley', 'parody',
        'prepay', 'purify', 'purvey', 'quarry', 'ramify', 'rarefy', 'rarify',
        'ratify', 'rebury', 'recopy', 'remedy', 'replay', 'sashay', 'scurry',
        'shimmy', 'shinny', 'steady', 'supply', 'survey', 'tumefy', 'typify',
        'uglify', 'verify', 'vilify', 'vinify', 'vivify', 'volley', 'waylay',
        'whinny', 'acetify', 'acidify', 'amnesty', 'amplify', 'atrophy', 'autopsy',
        'beatify', 'blarney', 'calcify', 'carnify', 'certify', 'clarify', 'company',
        'crucify', 'curtsey', 'dandify', 'destroy', 'dignify', 'disobey', 'display',
        'dulcify', 'falsify', 'fancify', 'fantasy', 'fortify', 'gainsay', 'glorify',
        'gratify', 'holiday', 'horrify', 'jellify', 'jollify', 'journey', 'justify',
        'lignify', 'liquefy', 'liquify', 'magnify', 'metrify', 'misally', 'misplay',
        'mollify', 'mortify', 'mummify', 'mystify', 'nigrify', 'nitrify', 'nullify',
        'opacify', 'outplay', 'outstay', 'overfly', 'overjoy', 'overlay', 'overpay',
        'petrify', 'pillory', 'portray', 'putrefy', 'qualify', 'rectify', 'remarry',
        'reunify', 'satisfy', 'scarify', 'signify', 'specify', 'stupefy', 'terrify',
        'testify', 'tourney', 'verbify', 'versify', 'vitrify', 'alkalify', 'ammonify',
        'beautify', 'bioassay', 'causeway', 'classify', 'corduroy', 'denazify', 'detoxify',
        'disarray', 'downplay', 'emulsify', 'esterify', 'etherify', 'fructify', 'gentrify',
        'humidify', 'identify', 'lapidify', 'misapply', 'miscarry', 'multiply', 'overplay',
        'overstay', 'prettify', 'prophesy', 'quantify', 'redeploy', 'revivify', 'rigidify',
        'sanctify', 'saponify', 'simplify', 'solidify', 'stratify', 'stultify', 'travesty',
        'underlay', 'underpay', 'accompany', 'butterfly', 'decalcify', 'decertify', 'demulsify',
        'demystify', 'denitrify', 'devitrify', 'disembody', 'diversify', 'electrify', 'exemplify',
        'frenchify', 'indemnify', 'intensify', 'inventory', 'microcopy', 'objectify', 'overweary',
        'personify', 'photocopy', 'preachify', 'preoccupy', 'speechify', 'syllabify', 'underplay',
        'blackberry', 'complexify', 'declassify', 'dehumidify', 'dillydally', 'disqualify',
        'dissatisfy', 'intermarry', 'oversupply', 'reclassify', 'saccharify', 'understudy',
        'hypertrophy', 'misidentify', 'oversimplify', 'transmogrify', 'interstratify',
    ]

    verb_with_ending_s = [
        'Bus', 'Gas', 'Bias', 'Boss', 'Buss', 'Cuss', 'Diss', 'Doss', 'Fuss', 'Hiss',
        'Kiss', 'Mass', 'Mess', 'Miss', 'Muss', 'Pass', 'Sass', 'Suds', 'Toss', 'Amass',
        'Bless', 'Class', 'Cross', 'Degas', 'Dress', 'Floss', 'Focus', 'Glass', 'Gloss',
        'Grass', 'Gross', 'Guess', 'Press', 'Truss', 'Access', 'Assess', 'Bypass', 'Callus',
        'Canvas', 'Caress', 'Caucus', 'Census', 'Chorus', 'Egress', 'Emboss', 'Harass',
        'Obsess', 'Precis', 'Recess', 'Rumpus', 'Schuss', 'Stress', 'Address', 'Aggress',
        'Callous', 'Canvass', 'Compass', 'Concuss', 'Confess', 'Degauss', 'Depress', 'Digress',
        'Discuss', 'Dismiss', 'Engross', 'Express', 'Harness', 'Impress', 'Nonplus', 'Oppress',
        'Percuss', 'Possess', 'Precess', 'Premiss', 'Process', 'Profess', 'Redress', 'Refocus',
        'Regress', 'Repress', 'Succuss', 'Summons', 'Surpass', 'Teargas', 'Trellis', 'Uncross',
        'Undress', 'Witness', 'Bollocks', 'Buttress', 'Compress', 'Distress', 'Outclass', 'Outguess',
        'Progress', 'Reassess', 'Suppress', 'Trespass', 'Waitress', 'Backcross', 'Embarrass',
        'Encompass', 'Overdress', 'Repossess', 'Reprocess', 'Unharness', 'Verdigris', 'Crisscross',
        'Decompress', 'Dispossess', 'Eyewitness', 'Misaddress', 'Overstress', 'Prepossess', 'Rendezvous',
        'Retrogress', 'Transgress', 'Disembarrass',
    ]

    common_prefixes = [
        'a', 'an', 'co', 'de', 'en', 'ex', 'il', 'im', 'in', 'ir', 'in',
        'un', 'up', 'com', 'con', 'dis', 'non', 'pre', 'pro', 'sub', 'sym',
        'syn', 'tri', 'uni', 'ante', 'anti', 'auto', 'homo', 'mono', 'omni',
        'post', 'tele', 'extra', 'homeo', 'hyper', 'inter', 'intra', 'intro',
        'macro', 'micro', 'trans', 'circum', 'contra', 'contro', 'hetero',
    ]

    common_prefix_trans = {
        'auto': (START_WORD, 'tự động'),
        'pre': (START_WORD, 'tiền/trước'),
    }

    noun_001 = 'sự/chỗ/phần/vùng/bản/cái/mức/độ/tính/sự/phép'
    noun_002 = 'mọi/nhiều/những/các/phần/bản/sự/chỗ'
    noun_003 = 'chủ nghĩa/tính/trường phái'
    noun_0004 = 'mọi/những chỗ/cái/các/nhiều/một số/vài vật/bộ/trình/người/viên/nhà/máy/phần/bản/cái/con/trình/bộ/người/viên/vật'
    adj_0001 = 'trong/thuộc/có tính/sự/chỗ/phần/trạng thái'
    adj_0002 = 'trong/là/nói một cách/có tính/theo'
    adv_0001 = 'đáng/có khả năng/thể'
    past_0001 = 'đã/bị/được'

    common_sufix_trans = {
        's': (START_WORD, noun_0004),
        'ed': (START_WORD, past_0001),
        'es': (START_WORD, noun_0004),
        'er': (END_WORD, 'hơn/trình/bộ/người/viên/nhà'),
        'ic': (START_WORD, 'giống/liên quan đến/hoạt động trong'),
        'or': (START_WORD, noun_0004),
        'al': (START_WORD, adj_0001),
        'inal': (START_WORD, adj_0001),
        'ly': (START_WORD, adj_0002),
        'ty': (START_WORD, adj_0001),
        '(s)': (START_WORD, 'những/các'),
        'ers': (START_WORD, noun_0004),
        'ies': (START_WORD, noun_0004),
        'ier': (END_WORD, 'hơn'),
        '\'s': (START_WORD, 'của'),
        'ors': (START_WORD, noun_0004),
        'est': (END_WORD, 'nhất'),
        'dom': (START_WORD, noun_001),
        'ful': (START_WORD, 'có/rất/nhiều'),
        'nce': (START_WORD, noun_001),
        'ily': (START_WORD, adj_0002),
        'ity': (START_WORD, noun_001),
        'ive': (START_WORD, adj_0001),
        'ish': (START_WORD, 'hơi hơi/có xu hướng/gần giống'),
        'ism': (START_WORD, noun_003),
        'isms': (START_WORD, noun_003),
        'als': (START_WORD, noun_0004),
        'ure': (START_WORD, noun_001),
        '\'ll': (START_WORD, 'sẽ'),
        'able': (START_WORD, adv_0001),
        'ably': (START_WORD, adv_0001),
        'ence': (START_WORD, noun_001),
        'doms': (START_WORD, noun_001),
        'ible': (START_WORD, adv_0001),
        'ibly': (START_WORD, adv_0001),
        'iest': (END_WORD, 'nhất'),
        'sion': (START_WORD, noun_001),
        'tion': (START_WORD, noun_001),
        'ness': (START_WORD, noun_001),
        'ency': (START_WORD, noun_001),
        'ment': (START_WORD, noun_001),
        'less': (START_WORD, 'vô/không/phi'),
        'like': (START_WORD, 'Thích/Giống Như/Tương Tự'),
        'than': (END_WORD, 'hơn'),
        'ures': (START_WORD, noun_001),
        'lable': (START_WORD, adv_0001),
        'ities': (START_WORD, noun_002),
        'iness': (START_WORD, noun_001),
        'ation': (START_WORD, noun_001),
        'ively': (START_WORD, adj_0002),
        'ments': (START_WORD, noun_001),
        'ption': (START_WORD, noun_001),
        'ations': (START_WORD, noun_002),
        'encies': (START_WORD, noun_002),
        'ization': (START_WORD, noun_001),
        'isation': (START_WORD, noun_001),
        'izations': (START_WORD, noun_002),
        'isations': (START_WORD, noun_002),
    }

    common_suffixes_replace_dict = {
        'a': list(sorted(
            [
                'ic',
            ],
            key=lambda x: len(x), reverse=True)),
        'e': list(sorted(
            ['able', 'ation', 'ations', 'ion', 'ions',
             'ity', 'ities', 'ing', 'ings', 'ously', 'ous', 'ive', 'ily',
             'ively', 'or', 'ors', 'iness', 'ature', 'er', 'en', 'ed', 'ied',
             'atures', 'ition', 'itions', 'itiveness',
             'itivenesses', 'itively', 'ative', 'atives',
             'ant', 'ants', 'ator', 'ators', 'ure', 'ures',
             'al', 'ally', 'als', 'iast', 'iasts', 'iastic', 'ial', 'y',
             'ary', 'ingly', 'ian', 'inal', 'ten', 'ize', 'ise',
             ],
            key=lambda x: len(x), reverse=True)),
        't': list(sorted(
            ['ce', 'cy', 'ssion', 'ssions', 'sion', 'sions'],
            key=lambda x: len(x), reverse=True)),
        'ce': list(sorted(
            ['t'],
            key=lambda x: len(x), reverse=True)),
        'x': list(sorted(
            ['ce', 'ces', ],
            key=lambda x: len(x), reverse=True)),
        'y': list(sorted(
            ['ies', 'ied', 'ier', 'iers', 'iest', 'ily', 'ic', 'ical', 'ically', 'iness', 'inesses',
             'ication', 'ications',
             ],
            key=lambda x: len(x), reverse=True)),
        'ix': ['ices'],
        'ion': ['ively'],
        'be': list(sorted(
            ['ption', 'ptions', ],
            key=lambda x: len(x), reverse=True)),
        'de': list(sorted(
            ['sible', 'sion', 'sions', 'sive'],
            key=lambda x: len(x), reverse=True)),
        'ce': list(sorted(
            ['tific', 'tist', 'tists'],
            key=lambda x: len(x), reverse=True)),  # science, scientific, scientist, scientists
        'ate': ['ant'],
        'cy': ['t'],
        'ze': ['s'],
        'te': list(sorted(
            ['cy', 'ry'],
            key=lambda x: len(x), reverse=True)),
        'le': ['ility'],
        'le': list(sorted(
            ['ility', 'ilities', ],
            key=lambda x: len(x), reverse=True)),
        'ic': list(sorted(
            ['ism', 'isms', 'on'],
            key=lambda x: len(x), reverse=True)),
        '': list(sorted(
            ['ed', 'ly', 'es'],
            key=lambda x: len(x), reverse=True)),
    }

    common_suffixes_replace_dict_sorted = list(common_suffixes_replace_dict.items())
    common_suffixes_replace_dict_sorted.sort(key=lambda x: len(x[0]))

    common_allowed_appostrophes = {
        "'": ['ll', 've', ', ', 's', 'd', ' ', '.']  # keep this sorted in length
    }

    common_suffixes = [
        'd', 'r', 'y', 's', 't', 'al', 'an', 'ce', 'cy', 'de', 'er', 'es', 'or', 'th', 'ic', 'ly',
        'ed', 'en', 'er', 'ic', 'ly', 'ry', 'st', 'ty', 'ze', 'ze', '\'s', '\'t', '\'m', 'als', 'ate',
        'age', 'aging', 'ages', 'ated', 'ates', 'ces', 'dom', 'ors', 'ers', 'est', 'eer', 'ial', 'ked',
        'ian', 'ism', 'ied', 'ier', 'iers', 'ion', 'ity', 'ics', 'ies', 'like', 'ful', 'less', 'ant',
        'ent', 'ary', 'ful', 'nce', 'ous', 'ive', 'ism', 'isms', 'ing', 'inal', 'ily', 'ity', 'ize',
        'ise', 'ish', 'ite', 'ful', 'ten', 'ual', 'ure', 'ous', '(s)', '\'re', '\'ve', '\'ll', 'n\'t',
        'ally', 'ator', 'ants', 'ance', 'doms', 'ence', 'ency', 'ents', 'ings', 'ures', 'ions', 'sion',
        'sions', 'sive', 'iest', 'iast', 'iasts', 'iastic', 'lier', 'less', 'liest', 'ment', 'ness',
        'ning', 'sion', 'ship', 'able', 'ably', 'ible', 'ical', 'ally', 'ious', 'less', 'ally', 'ward',
        'wise', 'ency', 'ators', 'sible', 'ively', 'ility', 'ually', 'ingly', 'ption', 'ation', 'iness',
        'ities', 'ition', 'itive', 'ments', 'sions', 'ssion', 'ships', 'aries', 'ature', 'ingly', 'izing',
        'ising', 'iness', 'ional', 'lable', 'ously', 'ptions', 'ility', 'ilities', 'itives', 'itions',
        'ication', 'ications', 'atures', 'ations', 'aceous', 'nesses', 'iously', 'ically', 'encies',
        'ssions', 'itively', 'ization', 'isation', 'itiveness', 'itivenesses', 'perception', 'perceive',
        'tific', 'tist', 'tists'
    ]

    common_infix = [
        '-',
    ]

    common_conjuctions = {
        'a minute later': '',
        'accordingly': '',
        'actually': '',
        'after': '',
        'after a while': '',
        'after a short time': '',
        'afterward': '',
        'also': '',
        'and': '',
        'another': '',
        'as an example': '',
        'as a result': '',
        'as soon as': '',
        'at last': '',
        'at length': '',
        'because': '',
        'because of this': '',
        'before': '',
        'besides': '',
        'briefly': '',
        'but': '',
        'consequently': '',
        'conversely': '',
        'equally': '',
        'finally': '',
        'first': '',
        'first of all': '',
        'first and last': '',
        'first time': '',
        'at first': '',
        'firstly': '',
        'for example': '',
        'for instance': '',
        'for this purpose': '',
        'for this reason': '',
        'fourth': '',
        'from here on': '',
        'further': '',
        'furthermore': '',
        'gradually': '',
        'hence': '',
        'however': '',
        'how are you': '',
        'in addition': '',
        'in conclusion': '',
        'in contrast': '',
        'in fact': '',
        'in short': '',
        'in spite of': '',
        'in spite of this': '',
        'despite of': '',
        'despite of this': '',
        'in summary': '',
        'in the end': '',
        'whereas': '',
        'whomever': '',
        'whoever': '',
        'in the meanwhile': '',
        'in the meantime': '',
        'in the same manner': '',
        'in the sameway': '',
        'just as important': '',
        'of equal importance': '',
        'on the contrary': '',
        'on the following day': '',
        'on the other hand': '',
        'other hands': '',
        'otherwise': '',
        'on purpose': '',
        'on the head': '',
        'hit the nail on the head': '',
        'least': '',
        'the least I can': '',
        'in the least': '',
        'last': '',
        'the last of': '',
        'last of all': '',
        'lastly': '',
        'later': '',
        'later on': '',
        'meanwhile': '',
        'moreover': '',
        'nevertheless': '',
        'next': '',
        'next to': '',
        'nonetheless': '',
        'now': '',
        'nor': '',
        'neither': '',
        'or': '',
        'when': '',
        'while': '',
        'presently': '',
        'second': '',
        'similarly': '',
        'since': '',
        'since then': '',
        'so': '',
        'so much': '',
        'so many': '',
        'soon': '',
        'so soon': '',
        'very soon': '',
        'as soon as possible': '',
        'as much as possible': '',
        'as many as possible': '',
        'as long as possible': '',
        'still': '',
        'subsequently': '',
        'such as': '',
        'such that': '',
        'as such': '',
        'the next week': '',
        'then': '',
        'thereafter': '',
        'there and then': '',
        'therefore': '',
        'and thus': '',
        'thus': '',
        'to be specific': '',
        'to begin with': '',
        'to be precise': '',
        'to be exact': '',
        'to illustrate': '',
        'to repeat': '',
        'to sum up': '',
        'too': '',
        'ultimately': '',
        'what': '',
        'with this in mind': '',
        'with that in mind': '',
        'yet': '',
        'not yet': '',
        'and yet': '',
        'although': '',
        'as if': '',
        'although': '',
        'as though': '',
        'even': '',
        'even if': '',
        'even though': '',
        'if': '',
        'if only if': '',
        'if only': '',
        'if when': '',
        'if then': '',
        'if you can': '',
        'if I can': '',
        'if it is possible': '',
        'inasmuch': '',
        'in order that': '',
        'just as': '',
        'lest': 'hầu cho không/e ngại/rằng',
        'now and then': '',
        'for now': '',
        'for now that is': '',
        'so for now': '',
        'but for now': '',
        'now since': '',
        'now that': '',
        'now that\'s what I call': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',
        # '': '',

    }
    common_sufix_translation = list(sorted(list(common_sufix_trans.items()), key=lambda x: len(x[0]), reverse=True))
    common_prefix_translation = list(sorted(list(common_prefix_trans.items()), key=lambda x: len(x[0]), reverse=True))

    ascending_sorted = list(sorted(common_prefixes))
    common_prefix_sorted = list(sorted(ascending_sorted, key=lambda x: len(x), reverse=False))

    ascending_sorted = list(sorted(common_suffixes))
    common_suffix_sorted = list(sorted(ascending_sorted, key=lambda x: len(x), reverse=False))

    ascending_sorted = list(sorted(common_infix))
    common_infix_sorted = list(sorted(ascending_sorted, key=lambda x: len(x), reverse=False))

    numberal = r"\b(one|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve|((thir|four|fif|six|seven|eigh|nine)teen)|((twen|thir|four|fif|six|seven|eigh|nine)ty)|(hundred|thousand|(mil|tril)lion))[s]?\b"
    # urlx_engine = URLX()

    NUMBERS = re.compile(r"^\s*(([\d]+)([\,\.]?[\s]?[\d]+)*)+$")

    REF_LINK_WITHOUT_REFWORD = re.compile(r'\<([^<]+)\>')
    PATH_CHAR = re.compile(r'[\\\/]')
    file_path_pattern_list = [
        # r'^$',
        r'^(([\w]+|[~\.]|[\.]{2})[:]?)?([/]([^\]+)?)+)$',
    ]

    DOS_COMMANDS = [
        "basica",
        "cd",
        "chdir",
        "chcp",
        "chkdsk",
        "cls",
        "comp",
        "ctty",
        "cv",
        "dblboot",
        "dblspace",
        "deltree",
        "dir",
        "diskcomp",
        "diskcopy",
        "doskey",
        "drvspace",
        "edlin",
        "emm386",
        "exe2bin",
        "fakemous",
        "fasthelp",
        "fastopen",
        "fc",
        "fdisk",
        "goto",
        "graftabl",
        "intersvr",
        "interlnk",
        "keyb",
        "loadfix",
        "loadhigh",
        "lh",
        "md",
        "mkdir",
        "mem",
        "memmaker",
        "msav",
        "msbackup",
        "mscdex",
        "msd",
        "msherc",
        "nlsfunc",
        "printfix",
        "qbasic",
        "rd",
        "rmdir",
        "recover",
        "rem",
        "ren",
        "setver",
        "smartdrv",
        "subst",
        "sys",
        "telnet",
        "truename",
        "ver",
        "vol",
        "vsafe",
        "xcopy",
    ]

    STB = r'[\"\'\(\<\{\[]'
    EDB = r'[\"\'\)\>\}\]]'
    # NUMB = r'([+-]?[\d]+(([\.\,][\d]+)*)+[\W]?)'
    NUMB = r"[+-]?[\d]+([\.\,][\d]+)?"
    # PATH = r'(([a-zA-Z][:]?)?) ([\\\/]+)(([\w-dd]+)?)*)'
    PATH = r'([a-zA-Z][:]?)?'
    MATH_OPS = r'[\s]?([\+\-\*\/\%\=x])[\s]?'
    runtime_ignore_list = None
    ignore_list = [
        r"\|([^\|]+)\|",    # ignore terms such as |TODO|
        r"^\w$",
        r"^(\w[\W]+)$",
        r"^\W?(\dD)\W?$",
        r"^\s*(" + NUMB + MATH_OPS + r".*" + NUMB + r")\s*$",
        # r"^\s*(" + STB + r"?(" + NUMB + r"(([\,]+[\s]+)?" + NUMB + r")*)+" + EDB + r"?)\s*$",
        # r"^\s*(" + STB + r"?([+-]?[\d]+)([\,\.]?[\s]?[\d]+)*)+" + EDB + r"?$",  # 1,000 or 0.00001 or 1, 2, 3, 4
        # r"^\s*(" + STB + r"?([\+\-]?[\d]+[\W]?)" + EDB + r"?)\s*$",  # (+-180°)
        # r"^\s*(" + STB + r"?[+-][\w]{1}[,.])*([\s]?[+-][\w]{1})" + EDB + r"?$",  # "+X, +Y, +Z, -X, -Y, -Z"
        r"^\s*(" + r"(cd|mk|mkdir)[\s]+" + r".*" + r")\s*$",
        r"^\s*(#fmod\(frame, 24\) / 24)\s*$",
        r"\S+\.\w+",  # file_name.png
        r"^\s*((GGX|Blender|GLSL|GPU)[s:]|Gamma[s:]?|Ge2Kwy5EGE0|Gizmo[s:]|GGX|GLSL|Gizmo[\s]?[\w]?)\s*$",
        r"^\s*(([\d]+([\.[\d]+)?)*(mil|mi|mm|km|cm|ft|m|yd|dm|st|pi))\s*$",
        r"^\s*(([\d]+(\.[\d]+)?)([\s]?[\/\+\-\*\%\=]?[\s]?([\d]+(\.[\d]+)?))*)\s*$",
        # r"^\s*(([\w]+)?([\.][\w]+)+)\s*$",  # bpy.context, bpy.context.object
        # r"^\s*(([\w]+)?([\.][\w]{3})+)\s*$",  # bpy.context, bpy.context.object
        r"^\s*(AAC|AVI Jpeg|antonioya|(Ctrl|Alt|Shift)\S*|AVX|AaBbCc|Albedo|Alembic|AC3|Alt|AMD|Ascii|AVX[\d]?|Acrylic)\s*$",
        r"^\s*(AVIJPEG|AVIRAW|BMP|DDS|DPX|IRIZ|JACK|JP2|RAWTGA|TGA|TIFF|[+-]<frame>|)\s*$",
        r"^\s*(Alpha|Alt|Apple macOS|Arch Linux|Ashikhmin-Shirley)\s*$",
        r"^\s*(B\-Spline|BSDF|BSSRDF|BU|BVH|Babel|Bezier|Bindcode|Bit[s]?|BkSpace|Bksp)\s*$",
        r"^\s*(Blackman\-Harris|Blosc|Barth|Byte\([s]*\)|Bytecode|Bézier|Backspace|(Blender\s(\d+[\d\.]+)))\s*$",
        r"^\s*(Bone[ABC]|COR\-|Cartesian|Bfont|ABC)\s*$",
        r"^\s*(Catmull\-(Clark|Rom)|Catrom|Chebychev|Clemens|Christensen\-Burley|Cineon|Collada)\s*$",
        r"^\s*(Cycles|Cycles:|Cinema(\s\(\d+\))?)\s*$",
        r"^\s*(DNxHD|DOF|Debian\/Ubuntu|Del|debian|Delta([\s][\w])?)\s*$",
        r"^\s*(Djv|Doppler|Dots\/BU|Dpi|DWAA)\s*$",
        r"^\s*(EWA|Epsilon|Embree|Esc|exr|FBX|Euler|FELINE|FFT|FSAA|Flash|FrameCycler|Français|msgfmt|fr_FR|Enter|Euler\s?\(?\w{1,3}?\)?|Float[\d]?)\s*$",
        r"^\s*(F[\d]{1,2})\s*$",  # F1-12
        r"^\s*(H\.264|Hosek \/ Wilkie|Houdini|HuffYUV|Hyperbolic[\s]?(Sine|Cosine)|Hosek \/ Wilkie(\s\d+)?|HDRI[s]?)\s*$",
        r"^\s*(ID|Ins|JPEG 2000|(ITU(\s\d+)?)|Internet[\w\W]|iScale)\s*$",
        r"^\s*(ITU \d+,)+",
        r"^\s*(KDE|K1, K2|Kirsch|komi3D)\s*$",
        r"^\s*(Lennard\-Jones|LimbNode|Laplace Beltrami|Lightwave Point Cache \(\.mdd\)|Linux|Look[\s]?Dev(HDRIs)?)\s*$",
        r"^\s*(MIS|MPlayer|my_app_template|(MS|Microsoft)?[-]?Windows|MacBook [\`]+Retina[\`]+|Makefile|Makefile|Manhattan|Matroska|Mega|Minkowski(\s[\d]+)?|Minkowski \d+\/\d+|Mitch|Mono|Musgrave)\s*$",
        r"^\s*(NDOF((\W)|[\s]?(ESC|Alt|Ctrl|Shift))?|hardware-ndof|NURBS|Nabla|Ndof|Nabla|Null|NVIDIA|nn|Nishita)\s*$",
        r"^\s*(OBJ|OSkey|Ogawa|Ogg[\s]?(Theora)?|Open(AL|CL|EXR|MP|Subdiv|VDB)+|Opus|ObData|ILM\'s OpenEXR|OpenEXR|Ozone|OptiX)\s*$",
        r"^\s*(P(CM|LY|NG)|Pack Bits|Poedit|Preetham|Prewitt|PBR|PolyMesh|PO|pip|pip3|PIZ|PXR24|pc2|Preetham(\s?\d+)?|Python(\:\s[\.\%s]+)?)\s*$",
        r"^\s*(Poedit|PIP|pagedown|pageup|pgdown|pgup|pip[\d]?|pot|print\(\))\s*$",
        r"^\s*(QuickTime|quasi\-)\s*$",
        r"^\s*(RK4|RRT|Redhat\/Fedora|rad_def|RLE)\s*$",
        r"^\s*(RONIN|Ryan Inch|Return)\s*$",
        r"^\s*(SDL|SSE[\d]+|STL|SVG|ShaderFX|Sigma|Sobel|Sobol|Stucci|Studio|Subversion|SubD|Subdiv|Silvio Falcinelli)\s*$",
        r"^\s*(Targa([\s]?Raw)?|Theora|TortoiseSVN|TxtIn|test1_|the|TAR-)\s*$",
        r"^\s*(TortoiseSVN|var[\s]+|wav)\s*$",
        r"^\s*(URL|UV[s:]?|(\w )?&( \w)?|Uber)\s*$",
        r"^\s*(VD16|VP9|VRML2|Verlet|Vorbis|Voronoi([\s]F[\d]([-]F[\d])?)?|)\s*$",
        r"^\s*(WEBM \/ VP9|Web(3D|M)|Win(tab)?|Windows Ink|WGT-|ZX)\s*$",
        r"^\s*(X(/Y|YZ)?|Xvid|XY|XZ|YCbCr(\s\(ITU\s?\d+?\))?)\s*$",
        r"^\s*(Y(CC)?|YCbCr(\s\(Jpeg\))?|Z(ip)?|ZIPS)\s*$",
        # r"^\s*([-]{2}([\w-]+)*)\s*$",
        r"^\s*([\"\'\*]?[\d]+(\.[\d]+)?([\s]?([K]?hz|bit[s]?))?[\"\'\*]?)\s*$",
        r"^\s*([\"\'][\.\s]?[\S]{1}[\"\'])\s*$",
        r"^\s*([\W]+)\s*$",
        r"^\s*([\d]+)(px|khz)?$",  # 1024x2048
        r"^\s*([\d]+[.,][\s]?)*[\d]+bit$",
        r"^\s*([\d]+[x][\d]+)\s*$",  # 1024x2048
        r"^\s*([\d]+\s?bit[s]?)\s*$",
        r"^\s*([\w\d]{2}\:){2}[\w\d]{2}\.[\w\d]{2}\.$",  # HH:MM:SS.FF
        r"^\s*([\w]([\s]+[\w\d])+)\s*$",  # :kbd:`S X 0`
        r"^\s*([^\S]{1})\s*$",  # single anything non spaces
        r"^\s*([^\w]+log.*wm.*)\s*$",
        r"^\s*(\"fr\"[:]?|\"fr\": \"Fran&ccedil;ais\"|)\s*$",
        r"^\s*(\%[d](x%[d])?)\s*$",  # %dx%d
        r"^\s*(\%\d+[\w]?)\s*$",  # %14s
        r"^\s*(\*\-[\d]+[\.][\w]{3})\s*$",  # *-0001.jpg
        # r"^\s*(\-\-render\-frame 1|\-(ba|con|noaudio|setaudio)|Mem)\s*$",
        r"^\s*(\.[\w]{2,5})\s*$",  # .jpg, .so
        r"^\s*(\d+[x]?)\s*$",  # 16x
        r"^\s*(_socket[\.](py|pyd)|Subversion|s\-leger|sequencer\-edit\-change|svn|meta-androcto|A_x\^2 \+ A_y\^2 \+ A_z\^2)\s*$",
        r"^\s*(bItasc|bin|bit[s]?|bl\*er|blendcache_[filename]anim_time_min|anim_time_max|anim_screen_switch|bl_math|bl_info|blender_docs|display_render|displayColor|blender_doc|blender_api)\s*$",
        r"^\s*(bpy\.(context|data|ops)|bpy\.([\w\.\-\_]+)|byte([s]?))\s*$",
        r"^\s*(cd|mkdir|ctrl)\s*$",
        r"^\s*(dam|deg|dev|developer\.blender\.org|dir\(\)|dm|dx)\s*$",
        r"^\s*(eevee|emission\(\)|esc|etc[\.]+)\s*$",
        r"^\s*(f\(\d+\)|fBM|flac|fr|fr\/|ft)\s*$",
        r"^\s*(git([\s]+[^\`]+)?|glTF 2\.0)\s*$",
        r"^\s*(hm|html|iTaSC|jpeg|SubRip)\s*$",
        r"^\s*(htt[ps][^\S]+)\s*$",
        r"^\s*(jpg|png)\s*$",
        r"^\s*(kConstantScope|kUniformScope|kUnknownScope|kVaryingScope|kVertexScope|kFacevaryingScope|kbd)\s*$",
        r"^\s*(mathutils|menuselection|microfacet_ggx\(N, roughness\)|microfacet_ggx_aniso\(N, T, ax, ay\))\s*$",
        r"^\s*(microfacet_ggx_refraction\(N, roughness, ior\)|mp[\d]+|msgstr|MPEG-4 \(divx\))\s*$",
        r"^\s*(mov|location[0]|cd|ch|hm|um|motorsep)\s*$",
        r"^\s*(oren_nayar\(N, roughness\)|wm\.operators\.\*|var all_langs \=(.*)|)\s*$",
        r"^\s*(quit\.blend|path:ray_length|render\-output\-postprocess|temp\-dir)\s*$",
        r"^\s*(rig_ui|roaoao|rotation_[xyz]|resolution_[xyz]|reflection\(N\)|rest_mat|rst|refraction\(N, ior\))\s*$",
        r"^\s*(the quick|brown fox|jumps over|the lazy dog)\s*$",
        r"^\s*Alembic([\s\W|abc]+)\s*$",
        r"^\s*Blender\([\s\d\.]+\)|Blender_id[\W]?|build\/html$",
        r"^(\s*MPEG([\-|\d]+)|MPEG H.264|MPEG-4\(DivX\))|MatCaps$",
        r"^\s*PAINT_GPENCILEDIT_GPENCILSCULPT_.*$",
        r"^\s*[\%s\s\'\:]+$",  # %s: %s
        r"^\s*[\-]*\d+(\.[\w]{2,5})\s*$",  # -0001.jpg
        r"^\s*[\W]{1}$",
        r"^\s*\%d(\s\w\s\%d)?(\W\s?)?$",  # %d x %d
        r"^\s*\*(\.[\w]{2,5})\s*$",  # *.jpg
        r"^\s*\.bashrc$",
        r"^\s*\:([\w\-\_]+)\:$",
        r"^\s*\:sup\:\`™\`$",
        r"^\s*\w([\s]?[<]?[\*\/\+\-\=][>]?[\s]?\w)+\s*$",  # A * B + C; A -> B
        r"^\s*\|([\w\-\_]+)\|$",
        r"^\s*\|[^\|]+\|$",  # |tick|cross|
        r"^\s*gabhead, Lell, Anfeo, meta-androcto$",
        r"^blender_api[:]?",
        r'^(A \(Alpha\))$',
        r'^(GPL[\s\w][\d][+])$',
        # r'^(\w+)(_\w+){1,}$',
        r'^File\:[^:]+\.\w+$',
        r'^(\|[\w]+([\-][\w]+)?.*(:kbd\:.*Alt-Backspace).*)$',  # |dagger|: ``|``, :kbd:`Alt-Backspace`, ``-``
        r'^[\W\d]+$',  # symbols and numbersr
        r'^[\w]\s?(\+|\-|\*|\/|\%|\=|\!\=|\>|\<|\>\=|\<\=|\=\=|\>\>|\<\<)\s?[\w]$',  # A - B, A >= B
        r'^\d+%s$' % (REF_MASK_STR * 2),  # internal ref placeholders
    ]

    FILE_EXTENSIONS = re.compile(r'^(\S+)?\.\w+$')
    DIGITS_ONLY = re.compile(r'^[\,\s\=\.\+\-e\d\*\/x×cdhbpyftmnck\[\]\(\)\>\<\{\}\\]+$')
    SINGLE_LETTER = re.compile(r'^\w$')
    KEYBOARD_MODIFIERS = re.compile(r'^(alt|ctrl|shift|end|home|pagup|pgdown|enter|delete|backspace|home)$', flags=re.I)
    # MAKE SURE all entries in this table are in LOWERCASE
    ignore_txt_list = [
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        "``rendering/``",
        " rgba byte",
        " rgb byte",
        "lambda:",
        " %s    x",
        " %s    y",
        " %s    z",
        "qbvh",
        "carve",
        "deflate",
        "bmesh",
        "radiance hdr",
        "blender",
        "laplacian",
        "boolean",
        " (boolean)",
        " + z",
        "#cos(frame)",
        "#frame / 20.0",
        "#python",
        "#sin(frame)",
        "%s ▶ %s",
        "(origin, vertex_coordinates)",
        "*undo*",
        "*x*\\ :sup:`e` + *y*\\ :sup:`e` + *z*\\ :sup:`e`",
        "+c",
        "+q1",
        "+q11",
        "+wt",
        "+wt2",
        ", rgb byte",
        ", rgba byte",
        "--log \"*undo*\"",
        "-e cycles",
        "-f -2",
        "-f 10",
        "-s 10 -e 500",
        "-t 2",
        "./datafiles/ ...",
        "./python/ ...",
        "1 - alpha",
        "1.0, 0.707 = sqrt(0.5), 0.354 = sqrt(2)/4, and 0.25",
        "16-bit",
        "1m 3mm",
        "1m, 3mm",
        "2.2mm + 5' / 3\" - 2yards",
        "20 °c",
        "2m 28.5cm",
        "32-bit",
        "4 × 8 bits",
        "4l",
        "5.969 - 0.215\\beta_{n} + 2.532\\beta_{n}^{2} -\n10.73\\beta_{n}^{3} + 5.574\\beta_{n}^{4} + 0.245\\beta_{n}^{5}\\right",
        "8-bit",
        "@ctrl",
        "@def",
        "[\"agent\"]",
        "[\"prop_name\"]",
        "\"agent\"",
        "\"fr\": \"fran&ccedil;ais\"",
        "\"u & v\"",
        "\\max(1.0 - melanin, 0.0001)",
        "\\n",
        "``.png``",
        "``.tga``",
        "``.xyz``",
        "``b``",
        "``g``",
        "``r``",
        "``send_field``",
        "a",
        "a, b",
        "addons_contrib",
        "albedo",
        "alembic",
        "anim_time_max",
        "anim_time_min",
        "anime",
        "anthony d'agostino",
        "antonio vazquez",
        "antonioya",
        "aov",
        "apic",
        "avi",
        "bind_mat",
        "bl_info",
        "blendcache_[filename]",
        "blender -d",
        "blender -r",
        "bpy",
        "bpy.",
        "bpy.app.debug = true",
        "brikbot",
        "bullseye",
        "bw",
        "catmull-clark",
        "catmull-rom",
        "cfl",
        "christensen-burley",
        "cm, m, ...",
        "collada, ...",
        "cor",
        "coreaudio",
        "cpu",
        "ctrl shift c",
        "cuda",
        "cuda+cpu",
        "cwolf3d",
        "damien picard",
        "daniel martinez lara",
        "dci-p3",
        "dealga mcardle",
        "def",
        "def-",
        "diffuse(n)",
        "distance^2",
        "diurnal",
        "dls",
        "doi 10.1111/j.1467-8659.2010.01805.x",
        "doi 10.1111/j.1467-8659.2010.01805.x",
        "doi:10.1111/j.1467-8659.2011.01976.x",
        "dolphindream",
        "dolphindream",
        "dommetysk",
        "dv",
        "eoan, focal",
        "exp",
        "exp(a)",
        "ffmpeg -b:v",
        "file:atvbuggy.zip",
        "folkert de vries",
        "functions.wolfram.com",
        "fweeb",
        "geo",
        "gltf 2.0 (.glb, .gltf)",
        "gltf",
        "gnome",
        "hc",
        "hdr",
        "height field",
        "henyey_greenstein(g)",
        "hip",
        "home",
        "hp reverb g2",
        "hs + v",
        "hsl",
        "hsv",
        "htc vive cosmos",
        "huawei",
        "hv + s",
        "ideasman42",
        "imaginer",
        "intel",
        "j2k",
        "julien deswaef",
        "k1 - k4",
        "kg/m\ :sup:`3`",
        "laplace",
        "length2dbp1d",
        "lichtso",
        "liero",
        "locale/fr",
        "location[0]",
        "loolarge",
        "lzw",
        "mano-wii",
        "manual/images",
        "marius giurgi",
        "matcap",
        "material:index",
        "mathvisprop",
        "mathvisstateproplist",
        "mattes",
        "mdd",
        "melanin :math:`0.25`",
        "melanin :math:`0.5`",
        "melanin :math:`0.75`",
        "melanin :math:`0`",
        "melanin :math:`1`",
        "meta-androcto",
        "metalliandy",
        "metaplane",
        "metathing",
        "mickael lozac'h et al",
        "monado",
        "motorsep",
        "mtext",
        "my_scripts",
        "node wrangler",
        "nouveau",
        "nuke",
        "object-proxy",
        "object:index",
        "object:location",
        "object:random",
        "object_fracture_cell",
        "oculus",
        "oha studio",
        "opencl+cpu",
        "operators.chain(), operators.bidirectional_chain()",
        "operators.sequential_split(), operators.recursive_split()",
        "org",
        "org-",
        "p1, p2",
        "paulo_gomes",
        "pdb/xyz",
        "phillips",
        "phymec",
        "phymec",
        "pioverfour",
        "pointcache (.pc2)",
        "pointcache",
        "pontiac",
        "pov-ray 3.7",
        "prefs-index",
        "prefs-menu",
        "prism",
        "pulseaudio",
        "quaternion",
        "qvt",
        "rgb",
        "rgba",
        "riff",
        "ring\\_count^{2} + ring\\_count",
        "rr",
        "rrggbb",
        "rto",
        "rv",
        "scene_linear",
        "scribus",
        "sculpt_mask_clear-data",
        "sdls",
        "sergey sharybin",
        "sid",
        "silvio falcinelli",
        "skinify guy",
        "sl",
        "stan pancakes",
        "stan pancakes",
        "stanford bunny",
        "steam",
        "steamvr",
        "subdivr",
        "subdivv",
        "sudo nano /etc/paths",
        "supported platforms",
        "sv + h",
        "svn add /path/to/file",
        "svn rm /path/to/file",
        "syrux",
        "tab",
        "the pixelary",
        "tissue",
        "tool-annotate",
        "tool-blade",
        "top -o %mem",
        "top -o mem",
        "topbar-app_menu",
        "topbar-render",
        "translucent(n)",
        "trumanblending",
        "trumanblending",
        "uberpov",
        "udims",
        "ui",
        "ui-eyedropper",
        "uni",
        "van der waals",
        "varkenvarken",
        "varkenvarken",
        "vector2d",
        "vesta",
        "vis",
        "vv",
        "wahooney",
        "wasapi",
        "x, y & z",
        "x, y, z",
        "x, z",
        "x3d",
        "xaire",
        "xr",
        "yadif",
        "yuv",
        "yz",
        "zanqdo",
        "zeffii",
    ]

    # def addIgnoreMathFunctions(self):
    #     math_func_list = dir(math)
    #     for name in math_func_list:
    #         is_funct =

    ignore_start_with_list = [
        # "bpy", "bpy", "bl_info", "dx",
        # "", "", "", "", "",
        # "", "", "", "", "", "", "", "", "", "", "", "", "",
        "MPEG-4(DivX)",
        "demohero, uriel, meta-androcto",
        "antonio vazquez (antonioya)",
        "vladimir spivak (cwolf3d)",
        "nuke (.chan)",
        # "a (alpha)",
        "(*x*\\ :sup:",
        "0 + (cos(frame / 8) * 4)",
        # "+x, +y, +z, -x, -y, -z",
        # "",
        # "",
    ]

    keep_list = [
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        # "",
        "2.25 -- october 2002:",
        "2.26 -- february 2003:",
        "2.27 -- may 2003:",
        "2.28x -- july 2003:",
        "`2.5x <https://www.blender.org/download/releases/#25-series-2009-2011>`__ -- from 2009 to august 2011:",
        "side-by-side",
        "supported platforms",
        "workbench also has an x-ray mode to see through objects, along with cavity and shadow shading to help display details in objects. workbench supports several lighting mechanisms including studio lighting and matcaps.",
        "more logs can be obtained by running blender from command line and using ``--factory-startup --debug-all`` flags. see :ref:`command_line-launch-index` and :ref:`command_line-args`.",
        "cycles allows you add cyclic motion to a curve that has two or more control points. the options can be set for before and after the curve.",
        "cycles applies a number of shader node optimizations both at compile time and run-time. by exploiting them it is possible to design complicated \"uber shader\" style node groups that incur minimal render time overhead for unused features.",
        "cycles debug",
        "cycles gets basic volumetric support on the cpu, more improvements to the motion tracker, two new modeling modifiers, some ui consistency improvements, and more than 560 bug fixes.",
        "cycles gets improved volumetric support, major upgrade to grease pencil, windows gets input method editors (imes) and general improvements to painting, freestyle, sequencer and add-ons.",
        "cycles gets volume and sss support on the gpu, pie menus are added and tooltips greatly improved, the intersection modeling tool is added, new sun beam node for the compositor, freestyle now works with cycles, texture painting workflow is improved, and more than 220 bug fixes.",
        "cycles has additional :ref:`visibility properties <render-cycles-object-settings-visibility>` and also grease pencil objects have additional :ref:`visibility properties <grease_pencil-object-visibility>`.",
        "cycles is blender's physically-based path tracer for production rendering. it is designed to provide physically based results out-of-the-box, with artistic control and flexible shading nodes for production needs.",
        "cycles shaders and lighting can be baked to image textures. this has a few different purposes, most commonly:",
        "cycles support for spherical stereo images for vr, grease pencil works more similar to other 2d drawing software, alembic import and export support, and improvements to bendy bones for easier and simpler rigging.",
        "cycles supports only object and collection render types.",
        "cycles supports three types of panoramic cameras; equirectangular, fisheye, and mirror ball. note that these cannot be displayed in non-rendered modes in the viewport, i.e. *solid* mode; they will only work for the final render.",
        "cycles the animation playback.",
        "cycles the frames in the sequence; restarting at frame one.",
        "cycles uses path tracing with next event estimation, which is not good at rendering all types of light effects, like caustics, but has the advantage of being able to render more detailed and larger scenes compared to some other rendering algorithms. this is because we do not need to store, for example, a photon map in memory, and because we can keep rays relatively coherent to use an on-demand image cache, compared to e.g. bidirectional path tracing.",
        "custom matcaps can be :ref:`loaded in the preferences <prefs-lights-matcaps>`.",
        "|aacute|: ``a``, :kbd:`alt-backspace`, ``'``",
        "|acircumflex|: ``a``, :kbd:`alt-backspace`, ``^``",
        "|agrave|: ``a``, :kbd:`alt-backspace`, ``\\``",
        "|aordinal|: ``a``, :kbd:`alt-backspace`, ``-``",
        "|aring|: ``a``, :kbd:`alt-backspace`, ``o``",
        "|ash|: ``a``, :kbd:`alt-backspace`, ``e``",
        "|atilde|: ``a``, :kbd:`alt-backspace`, ``~``",
        "|ccedilla|: ``c``, :kbd:`alt-backspace`, ``,``",
        "|cent|: ``c``, :kbd:`alt-backspace`, ``|``",
        "|copyright|: ``o``, :kbd:`alt-backspace`, ``c``",
        "|cross|",
        "|dagger|: ``|``, :kbd:`alt-backspace`, ``-``",
        "|division|: ``-``, :kbd:`alt-backspace`, ``:``",
        "|doubledagger|: ``|``, :kbd:`alt-backspace`, ``=``",
        "|euml|: ``e``, :kbd:`alt-backspace`, ``\"``",
        "|half|: ``1``, :kbd:`alt-backspace`, ``2``",
        "|none|",
        "|oslash|: ``o``, :kbd:`alt-backspace`, ``/``",
        "|plusminus|: ``-``, :kbd:`alt-backspace`, ``+``",
        "|registered|: ``o``, :kbd:`alt-backspace`, ``r``",
        "|section|: ``s``, :kbd:`alt-backspace`, ``s``",
        "|tick|",
        "|todo|",
        "|trademark|: ``t``, :kbd:`alt-backspace`, ``m``",
        "**1.30 -- april 1998:** linux and freebsd version, port to opengl and x11.",
        "add-ons.",
        "height field",
        "|tick|",
        "|cross|",
        "feature/engine/support",
        "node wrangler",
        "|todo|",
        "cycles the animation playback.",
        "non-chaining",
        "undo/redo/history",
        "models/materials/brushes",
        "cycles modifier",
        "cycles render device",
        "0D",
        "de",
        "(de)",
        "(de)select first/last",
        "upper_arm",
        "switching/enabling/disabling",
        "toggle/enable/disable",
        "cycles only",
        "yellow/green/purple",
        "/base",
        "_right",
        "_left",
        "vertices/edges/faces",
        "(translation/scale/rotation)",
        "translation/scale/rotation",
        "path/curve-deform",
        "a",
        "an",
        "etc",
        "etc.",
        "e.g",
        "e.g.",
        "i.e",
        "i.e.",
        "add-on",
        "off-axis",
        "toe-in",
        "sub-target",
        "foam + bubbles",
        "spray + foam + bubbles",
        "fire + smoke",
        "sub-steps",
        "z-axis",
        "normal/view"
        "f-curve",
        "f-modifier",
        "counter-clockwise",
        "normal/surface",
        "left/right",
        "top/bottom",
        "link/append",
        "fog/mist",
        "exterior/interior",
        "flat/smooth",
        "mirror%s",
        "un-subdivide",
        "un-comment",
        "shrink/fatten",
        "smoke + fire",
        "expand/contract",
        "open/close",
        "hide/show",
        "co-planar",
        "hide/unhide",
        "lock/unlock",
        "major/minor",
        "click-extrude",
        "front/back",
        "rotation/scale",
        "blender_version",
        "mpeg preseek",
        "0 or 1",
        "anti-aliases",
        "anti-aliased",
        "anti-aliasing",
        "marker-and-cell grid",
        "reflect/refract",
        "scattering/absorbtion",
        "inside/outside",
        "dots/bu",
        "model by © 2016 pokedstudio.com",
        "video: from blender 1.60 to 2.50",
        "right-click-select",
        # "",
        # "",
    ]

    reverse_order_list = [
        r'\"cơ sở -- basis\"',
        r'^\"bone\"$',
        r'^\"xương -- bone\"$',
        r'^\"xương\"$',
        r'bone\.[\d]+',
        r'khóa.*[\d]+(\s[\-]{2}\s(key))',
        r'xương\.[\d]+',
    ]

    keep_contains_list = [
        "/etc",
        "april",
        "august",
        "bone",
        "december",
        "e.g.",
        "etc.",
        "february",
        "hh:mm:ss.ff",
        "i.e.",
        "january",
        "july",
        "june",
        "march",
        "may",
        "novemeber",
        "october",
        "september",
        "xương",
        # "",
        # "",
    ]

    symbol_splitting_pattern_list = [
        NON_SPACE_SYMBOLS,
        SYMBOLS,
        UNDER_SCORE,
    ]

    keep_contains_list.sort()
    keep_list.sort()

    complex_pattern_type_list = [
        RefType.ABBR,
        RefType.ATTRIB,
        RefType.DOC,
        RefType.FUNC,
        RefType.GA,
        RefType.GA_EMBEDDED_GA,
        RefType.GA_EMBEDDED_GA,
        RefType.GENERIC_DOUBLE_GA,
        RefType.GENERIC_DOUBLE_GA,
        RefType.GENERIC_REF,
        RefType.GENERIC_REF,
        RefType.GENERIC_SINGLE_GA,
        RefType.GENERIC_SINGLE_GA,
        RefType.GUILABEL,
        RefType.KBD,
        RefType.MENUSELECTION,
        RefType.METHOD,
        RefType.MOD,
        RefType.REF,
        RefType.SINGLE_GA,
        RefType.SUP,
        RefType.TERM,
    ]

    pattern_list = [
        (PYTHON_FORMAT, RefType.PYTHON_FORMAT),
        (FUNCTION, RefType.FUNCTION),
        (DBL_QUOTE, RefType.DBL_QUOTE),
        (SNG_QUOTE, RefType.SNG_QUOTE),
        (DBL_AST_QUOTES, RefType.AST_QUOTE),
        (SNG_AST_QUOTE, RefType.AST_QUOTE),
        (GA_SYMB_LEADING, RefType.GENERIC_DOUBLE_GA),
        # (GA_ONLY, RefType.GENERIC_DOUBLE_GA),
        # (GA_INTERNAL_LINK, RefType.SINGLE_GA),
        (GA_DOUBLE, RefType.DOUBLE_GA),
        (BLANK_QUOTE, RefType.BLANK_QUOTE),
        (ATTRIB_REF, RefType.ATTRIB),
        (GA_GENERIC_DOUBLE, RefType.GENERIC_DOUBLE_GA),
        (GA_GENERIC_SINGLE, RefType.GENERIC_SINGLE_GA),
        (REF_GENERIC, RefType.GENERIC_REF),
        (ARCH_BRAKET_SINGLE_FULL, RefType.ARCH_BRACKET),
    ]
    removable_txt = r'%(\S+)'
    removable_end = r'(%s)$' % (removable_txt)
    removable_start = r'^(%s)' % (removable_txt)

    REMOVABLE_START = re.compile(removable_start, re.I)
    REMOVABLE_END = re.compile(removable_end, re.I)
    REMOVABLE_PAT = re.compile(removable_txt, re.I)

    must_be_upper_case = [
        'GNU',
        'GPL',
        'HTML',
        'RGB',
        'RGBA',
        'Alpha',
        'PDF',
        'UV',
        '2D',
        '3D',
        '4D',
    ]

    reserved_txt = r'(e\.g\.)|' \
                   r'(i\.e\.)|' \
                   r'(etc\.)|' \
                   r'((Ref|Irr|Mr|Dr|Ms|Fig|vs|GPU|GPL)\.)|' \
                   r'(v\.v\.+)|' \
                   r'({[^{}]+})|' \
                   r'(\|[^\|]+\|)|' \
                   r'(\%[\d\.dsf]+(\%+)?)|' \
                   r'(@{[^{}@]+})|' \
                   r'(![^!]+!)|' \
                   r'(\\?[\+\-]+)|(\?+)|(\_[\#]+)|([\%\º])|' \
                   r'(\[[^\[\]]+\])'
    reserved_txts = r'(%s)' % (reserved_txt)
    RESERVED_TXTS = re.compile(reserved_txt, re.I)

    pattern_list_with_reserved = [
        (PYTHON_FORMAT, RefType.PYTHON_FORMAT),
        (SNG_QUOTE, RefType.SNG_QUOTE),
        (DBL_QUOTE, RefType.DBL_QUOTE),
        (SNG_AST_QUOTE, RefType.AST_QUOTE),
        (GA_DOUBLE_EMBEDDED_GA, RefType.GA_EMBEDDED_GA),
        (GA_INTERNAL_LINK, RefType.SINGLE_GA),
        (GA_DOUBLE, RefType.DOUBLE_GA),
        (BLANK_QUOTE, RefType.BLANK_QUOTE),
        (ATTRIB_REF, RefType.ATTRIB),
        (GA_GENERIC_DOUBLE, RefType.GENERIC_DOUBLE_GA),
        (GA_GENERIC_SINGLE, RefType.GENERIC_SINGLE_GA),
        (REF_GENERIC, RefType.GENERIC_REF),
        (RESERVED_TXTS, RefType.RESERVED),
        (ARCH_BRAKET_SINGLE_FULL, RefType.ARCH_BRACKET),
    ]

    pattern_list_complex = [
        (re.compile('\s'), RefType.REMOVE_ORIGINAL),
        (PYTHON_FORMAT, RefType.PYTHON_FORMAT),
        (FUNCTION, RefType.FUNCTION),
        (GA_DOUBLE_EMBEDDED_GA, RefType.GA_EMBEDDED_GA),
        (GA_INTERNAL_LINK, RefType.SINGLE_GA),
        (GA_DOUBLE, RefType.DOUBLE_GA),
        (ATTRIB_REF, RefType.ATTRIB),
        (GA_GENERIC_DOUBLE, RefType.GENERIC_DOUBLE_GA),
        (GA_GENERIC_SINGLE, RefType.GENERIC_SINGLE_GA),
        (REF_GENERIC, RefType.GENERIC_REF),
    ]

    ref_type_unquoteable = [
        RefType.ARCH_BRACKET,
        RefType.SNG_QUOTE,
        RefType.DBL_QUOTE,
        RefType.AST_QUOTE,
    ]

    no_bracket_pattern_list = [
        (PYTHON_FORMAT, RefType.PYTHON_FORMAT),
        (FUNCTION, RefType.FUNCTION),
        (SNG_QUOTE, RefType.SNG_QUOTE),
        (DBL_QUOTE, RefType.DBL_QUOTE),
        (SNG_AST_QUOTE, RefType.AST_QUOTE),
        (GA_DOUBLE_EMBEDDED_GA, RefType.GA_EMBEDDED_GA),
        (GA_INTERNAL_LINK, RefType.SINGLE_GA),
        (GA_DOUBLE, RefType.DOUBLE_GA),
        (BLANK_QUOTE, RefType.BLANK_QUOTE),
        (ATTRIB_REF, RefType.ATTRIB),
        (GA_GENERIC_DOUBLE, RefType.GENERIC_DOUBLE_GA),
        (GA_GENERIC_SINGLE, RefType.GENERIC_SINGLE_GA),
        (REF_GENERIC, RefType.GENERIC_REF),
    ]

    no_bracket_and_quoted_pattern_list = [
        (PYTHON_FORMAT, RefType.PYTHON_FORMAT),
        (FUNCTION, RefType.FUNCTION),
        (BLANK_QUOTE, RefType.BLANK_QUOTE),
        (ATTRIB_REF, RefType.ATTRIB),
        (GA_DOUBLE_EMBEDDED_GA, RefType.GA_EMBEDDED_GA),
        (GA_GENERIC_DOUBLE, RefType.GENERIC_DOUBLE_GA),
        (GA_GENERIC_SINGLE, RefType.GENERIC_SINGLE_GA),
        (REF_GENERIC, RefType.GENERIC_REF),
    ]

    clean_out_symb_list = [
        str(RefType.AST_QUOTE.value),
        str(RefType.DBL_AST_QUOTE.value),
        str(RefType.SNG_QUOTE.value),
    ]
    jlist = "\\".join(clean_out_symb_list)
    clean_out_symb_list_pat_txt = r'[%s]' % (jlist)
    not_clean_out_symb_list_pat_txt = r'[^%s]+' % (jlist)
    NOT_SPECIAL_QUOTED_PATTERN = re.compile(not_clean_out_symb_list_pat_txt)

    pattern_list_absolute = [
        ARCH_BRAKET_SINGLE_ABS,
        PYTHON_FORMAT_ABS,
        FUNCTION_ABS,
        SNG_AST_QUOTE,
        DBL_QUOTE_ABS,
        SNG_QUOTE_ABS,
        BLANK_QUOTE_ABS,
        ATTRIB_REF_ABS,
    ]
    ga_go_first_list = [
        'min',
        'max',
        'radians',
        'degrees',
        'abs',
        'fabs',
        'floor',
        'ceil',
        'trunc',
        'round',
        'int',
        'sin',
        'cos',
        'tan',
        'asin',
        'acos',
        'atan',
        'atan2',
        'exp',
        'log',
        'sqrt',
        'pow',
        'fmod',
        'and',
        'or',
        'not',
        'true',
        'false',
        'alpha',
        'Front\(X\-Z\)',
        'frame\/8',
        'file\.py',
        'roty\,movx',
        '^(pdt\_)$',
        '25t 20mm gear',
        'gears \– 20mm 25teeth',
        'x rot',
        'y rot',
        'z rot',
        'aim',
        'polyline',
        '\(lw\)polyline',
        '\(lw\)polygon',
        'line',
        'delta',
        '^([-]+\S+)$',
        '(^[\/\.]+)|([\/]+$)',
        '^\@?(CTRL|MCH|DEF)$',
        '^((\w+\_)\w+)+$',
        '^(Return)$',
        'analemma',
        # '',
        # '',
        # '',
        # '',
        # '',
        # '',
        # '',
        # '',
        # '',
        # '',
        # '',
        # '',
    ]
    temp_list = []
    for pat in ga_go_first_list:
        is_start = pat.startswith('^')
        is_end = pat.endswith('$')
        insert_start_end = not (is_start or is_end)
        if insert_start_end:
            pat_txt = r'^(%s)$' % (pat)
        else:
            pat_txt = r'(%s)' % (pat)
        temp_list.append(pat_txt)
    ga_go_first_pat_txt = '|'.join(temp_list)
    GA_GO_FIRST = re.compile(ga_go_first_pat_txt, flags=re.I)

    global_ref_map = None
    ss_map = {
        "Syncing ${}": "${} đồng bộ hóa",
        "System ${}": "${} hệ thống",
        "${} Mode": "Chế độ ${}",
        "Target ${}": "${} mục tiêu",
        "Target for constraints": "Mục tiêu cho các ràng buộc",
        "Target/Owner": "Mục tiêu/Chủ sở hữu",
        "Teapot by Anthony D'Agostino": "Ấm trà của Anthony D'Agostino",
        "Technical ${}": "${} kỹ thuật",
        "Template ${}": "${} Mẫu",
        # "${} and ${}": "${} và ${}",
        # "${} for ${}": "${} cho/đối với ${}",
        # "${} model": "mô hình ${}",
        # "${} origin": "tọa độ gốc của ${}",
        # "${} transformation(\\w)?": "các/những biến hóa ${}",
        # "${} view": "góc nhìn của ${}",
        # "if ${}": "nếu ${}",
        # "if in ${}": "nếu trong ${}",
        # "in ${}": "trong ${}",
        # "object ${}": "${} của vật thể",
        # "only ${}": "duy ${} mà thôi",
        # "only for ${}": "chỉ dành cho/sử dụng trong ${} mà thôi",
        # "only in ${}": "chỉ trong ${} mà thôi",
        # "or ${} if ${}": "hoặc ${} nếu ${}",
        # "${1/ED(ing)} ${2/ED(ing)} ${3/NP/NC/MX1}": "${1} ${3} ${2}",
        # "${1/ED(ing)} ${2/NP/NC}": "${2} ${1}",
        # "${1/ED(ing)} ${2}": "${2} ${1}",
        # "${1} ${2/ED(ies)}": "${2} ${1}",
        # "${ED(\\?)}": "${} phải không?",
        # "${ED(ed)} cycle(\\w)?": "chu trình ${}",
        # "${NP/NC/MX1} Constraint": "ràng buộc ${}",
        # "${NP/NC/MX2} goal": "@{des} ${}",
        # "${NP/NC/MX2} modifier(\\w+)?": "bộ điều chỉnh ${}",
        # "${NP/NC/MX3} bone": "xương ${}",
        # "${NP/NC} a set of ${NP/NC}": "${} một bộ ${}",
        # "${NP/NC} with ${NP/NC}": "${} với ${}",
        # "${NP} mode": "chế độ ${}",
        # "${NP} strip": "dải ${}",
        # "${} (am|are|is|was|were) not ${}": "${} đã không phải là/được ${}",
        # "${} (a|an) ${}": "${} một ${}",
        # "${} alone": "đơn điệu/độc/một mình ${}",
        # "${} and ${}": "${} và/đồng thời/rồi ${}",
        # "${} appl(y|ied|ies|ying)": "${} được áp dụng cả",
        # "${} buttons": "các nút bấm ${}",
        # "${} by ${}": "${} bằng/bởi/theo ${}",
        # "${} coordinate(s)?": "các tọa độ ${}",
        # "${} field": "ô ${}",
        # "${} for example": "lấy ví dụ, ${}, chẳng hạn",
        # "${} from ${}": "${} từ ${} vậy",
        # "${} icon": "biểu tượng ${}",
        # "${} icons": "các biểu tượng ${}",
        # "${} in ${}": "${} trong ${}",
        # "${NP/NC/MX2} modifier(\\w+)?": "bộ điều chỉnh ${}",
        # "${} is ${}": "${} thì/là/được ${}",
        # "${} of ${}": "${} của ${}",
        # "${} on ${}": "${} trên/ở/lên/vào/về ${}",
        # "${} only ${}": "duy ${} ${} mà thôi",
        # "${} or ${}": "${} hoặc ${}",
        # "${} orientation": "định hướng ${}",
        # "${} panel": "bảng ${}",
        # "${} per ${}": "${} mỗi một ${}",
        # "${} to ${}": "${} với/vào ${}",
        # "${} transformation(s)?": "các biến hóa về ${}",
        # "${} which  ${}": "${} tức ${}",
        # "${} with ${}": "${} với ${}",
        # "${}(\\W)? etc\\.)": "${}, v.v.)",
        # "(a|an) ${}": "một ${}",
        # "(called|known|named|namely)(\\W)? ${}": "được gọi/có tên là ${}",
        # "after ${} is called": "sau khi ${} đã được gọi",
        # "all ${}": "toàn bộ ${}",
        # "and some ${}": "và một số/vài các ${}",
        # "any ${NP/NC}": "bất cứ ${} nào",
        # "current ${}": "${} hiện tại",
        # "e\\.g\\. ${}": "@{eg}",
        # "e\\.g\\: ${}": "@{eg}",
        # "enable ${}": "bật ${} lên",
        # "enabled ${}": "${} đã được bật lên",
        # "equal to ${}": "bằng ${}",
        # "equal to ${}": "là bằng ${}",
        # "everything ${}": "mọi cái ${}",
        # "except ${}": "ngoại trừ ${}",
        # "factor = ${MX1}": "hệ số = ${}",
        # "factor of ${NP/NC}": "hệ số ${}",
        # "faster ${NP/NC}": "${} nhanh hơn",
        # "flexible ${}": "${} linh động/hoạt",
        # "for ${NP/NC}": "để đạt được/dành cho/đối với ${}",
        # "for example\\W? ${}": "lấy ví dụ, ${}, chẳng hạn",
        # "human ${NC}": "${} của con người",
        # "i\\.e ${}": "@{ie}",
        # "i\\.e\\. ${}": "@{ie}",
        # "i\\.e\\. for ${}": "@{ie} cho/đối với ${}",
        # "i\\.e\\; ${}": "@{ie}",
        # "if ${} alreay ${}": "nếu ${} đã ${}",
        # "if ${} do not alreay ${}": "nếu ${} chưa ${}",
        # "if ${} is set to ${}": "nếu ${} được bố trí/đặt là ${}",
        # "if they ${}": "nếu chúng ${}",
        # "in ${} menu": "trong trình đơn ${}",
        # "in ${} mode": "trong chế độ ${}",
        # "in ${} panel": "trong bảng ${}",
        # "in which case ${NP}": "nếu trong trường hợp đó thì ${}",
        # "inside ${}": "nội bên trong ${}",
        # "is equal to ${}": "là bằng ${}",
        # "less ${NP/NC}": "ít ${} hơn",
        # "like ${} or ${}": "như ${} hoặc ${} vậy",
        # "like ${}": "tựa như ${} vậy",
        # "made by ${}": "do ${} tạo ra",
        # "manually ${}": "${} một cách thủ công",
        # "more ${ED(\\?)}": "nhiều ${} hơn, chăng?",
        # "more ${NP/NC}": "nhiều ${} hơn",
        # "more ${} but ${}": "nhiều ${} hơn song ${}",
        # "more ${} than": "nhiều ${} hơn",
        # "more ${} to ${}": "nhiều ${} hơn với ${}",
        # "most ${}": "hầu hết/rất ${}",
        # "most of the time ${}": "hầu như/thường là ${}",
        # "multipl(y|ies|ied) by ${}": "nhân với ${}",
        # "multiple ${}": "đa/rất nhiều ${}",
        # "must ${}": "@{must} ${}",
        # "must be ${}": "@{must} phải được/là ${}",
        # "negative ${}": "${} âm",
        # "neither of ${} is ${}": "không có ${} là ${}",
        # "neither of which is ${}": "không có nào là ${}",
        # "never ${}": "không bao giờ ${}",
        # "next ${}": "${} tiếp theo",
        # "no ${NC} yet": "hiện chưa có ${}",
        # "no ${NP/NC}": "không có/được/phải ${} nào cả",
        # "no ${} on ${}": "không có/được/phải ${} trên ${}",
        # "not ${NC} yet": "chưa ${}",
        # "not for ${}": "không sử dụng cho ${}",
        # "previous ${NP/NC}": "${} đứng trước/trước đây",
        # "selected ${}": "${} đã được lựa chọn",
        # "some ${}": "một số/vài các ${}",
        # "that doesn't have ${NP}": "tức cái không có ${}",
        # "the ${} goal": "@{des} ${}",
        # "the ${} one": "cái ${}",
        # "those ${}": "những/các ${} ấy",
        # "those whose ${}": "những/các ${} ấy của chúng",
        # "very ${}": "rất/vô cùng/hoàn toàn ${}",
        # "which doesn't have ${NP}": "mà/tức cái không có ${}",
        # "while ${}": "trong khi đang ${}",
        # "whose ${}": "${} của chúng",
    }


class SentStructModeRecord:
    def __init__(self, smode_txt=None, smode=None, extra_param=None):
        self.smode_txt: str = smode_txt
        self.smode: SentStructModeRecord = smode
        self.extra_param = extra_param


class SentStructMode(Enum):
    ANY = Definitions.ANY
    EXCLUDE = Definitions.EXCLUDE
    NOT_TRAILING = Definitions.NOT_TRAILING
    NOT_LEADING = Definitions.NOT_LEADING
    EQUAL = Definitions.EQUAL
    TRAILING_WITH = Definitions.TRAILING_WITH
    LEADING_WITH = Definitions.LEADING_WITH
    EMBEDDED_WITH = Definitions.EMBEDDED_WITH
    PATTERN = Definitions.PATTERN
    NUMBER_ONLY = Definitions.NUMBER_ONLY
    POSITION_PRIORITY = Definitions.POSITION_PRIORITY
    ORDERED_GROUP = Definitions.ORDERED_GROUP
    NO_PUNCTUATION = Definitions.NO_PUNCTUATION
    MAX_UPTO = Definitions.MAX_UPTO
    NO_CONJUNCTIVES = Definitions.NO_CONJUNCTIVES
    NO_FULL_STOP = Definitions.NO_FULL_STOP

    @classmethod
    def getName(cls, string_value: str):
        for name, member in cls.__members__.items():
            is_any = (member == cls.ANY)
            if is_any:
                continue

            is_match = (member.value.search(string_value) is not None)
            if bool(is_match):
                return member
        return cls.ANY
