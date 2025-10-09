#!/usr/bin/env python3
"""
RirePE Packet Monitor - Python CLI Client
Connects to the RirePE DLL via TCP to log and send packets
"""

import socket
import struct
import sys
import argparse
from enum import IntEnum
from datetime import datetime

# Server->client opcode mappings from sendops-92.properties
RECV_OPCODES = {
    0: "LOGIN_STATUS",
    1: "GUEST_ID_LOGIN",
    2: "ACCOUNT_INFO",
    3: "SERVERSTATUS",
    4: "GENDER_DONE",
    5: "CONFIRM_EULA_RESULT",
    6: "CHECK_PINCODE",
    7: "UPDATE_PINCODE",
    8: "VIEW_ALL_CHAR",
    9: "SELECT_CHARACTER_BY_VAC",
    10: "SERVERLIST",
    11: "CHARLIST",
    12: "SERVER_IP",
    13: "CHAR_NAME_RESPONSE",
    14: "ADD_NEW_CHAR_ENTRY",
    15: "DELETE_CHAR_RESPONSE",
    16: "CHANGE_CHANNEL",
    17: "PING",
    18: "KOREAN_INTERNET_CAFE_SHIT",
    20: "CHANNEL_SELECTED",
    21: "HACKSHIELD_REQUEST",
    22: "RELOG_RESPONSE",
    25: "CHECK_CRC_RESULT",
    26: "LAST_CONNECTED_WORLD",
    27: "RECOMMENDED_WORLD_MESSAGE",
    29: "CHECK_SPW_RESULT",
    30: "INVENTORY_OPERATION",
    31: "INVENTORY_GROW",
    32: "STAT_CHANGED",
    33: "GIVE_BUFF",
    34: "CANCEL_BUFF",
    35: "FORCED_STAT_SET",
    36: "FORCED_STAT_RESET",
    37: "UPDATE_SKILLS",
    38: "SKILL_USE_RESULT",
    39: "FAME_RESPONSE",
    40: "SHOW_STATUS_INFO",
    41: "OPEN_FULL_CLIENT_DOWNLOAD_LINK",
    42: "MEMO_RESULT",
    43: "MAP_TRANSFER_RESULT",
    44: "ANTI_MACRO_RESULT",
    46: "CLAIM_RESULT",
    47: "CLAIM_AVAILABLE_TIME",
    48: "CLAIM_STATUS_CHANGED",
    49: "SET_TAMING_MOB_INFO",
    50: "QUEST_CLEAR",
    51: "ENTRUSTED_SHOP_CHECK_RESULT",
    52: "SKILL_LEARN_ITEM_RESULT",
    53: "GATHER_ITEM_RESULT",
    54: "SORT_ITEM_RESULT",
    56: "SUE_CHARACTER_RESULT",
    58: "TRADE_MONEY_LIMIT",
    59: "SET_GENDER",
    60: "GUILD_BBS_PACKET",
    62: "CHAR_INFO",
    63: "PARTY_OPERATION",
    65: "EXPEDITION_RESULT",
    66: "BUDDYLIST",
    68: "GUILD_OPERATION",
    69: "ALLIANCE_OPERATION",
    70: "SPAWN_PORTAL",
    71: "OPEN_GATE",
    72: "SERVERMESSAGE",
    73: "INCUBATOR_RESULT",
    74: "SHOP_SCANNER_RESULT",
    75: "SHOP_LINK_RESULT",
    76: "MARRIAGE_REQUEST",
    77: "MARRIAGE_RESULT",
    78: "WEDDING_GIFT_RESULT",
    79: "NOTIFY_MARRIED_PARTNER_MAP_TRANSFER",
    80: "CASH_PET_FOOD_RESULT",
    81: "SET_WEEK_EVENT_MESSAGE",
    82: "SET_POTION_DISCOUNT_RATE",
    83: "BRIDLE_MOB_CATCH_FAIL",
    84: "IMITATED_NPC_RESULT",
    85: "IMITATED_NPC_DATA",
    86: "LIMITED_NPC_DISABLE_INFO",
    87: "MONSTER_BOOK_SET_CARD",
    88: "MONSTER_BOOK_SET_COVER",
    89: "HOUR_CHANGED",
    90: "MINIMAP_ON_OFF",
    91: "CONSULT_AUTHKEY_UPDATE",
    92: "CLASS_COMPETITION_AUTHKEY_UPDATE",
    93: "WEB_BOARD_AUTHKEY_UPDATE",
    94: "SESSION_VALUE",
    95: "PARTY_VALUE",
    96: "FIELD_SET_VARIABLE",
    97: "BONUS_EXP_RATE_CHANGED",
    98: "POTION_DISCOUNT_RATE_CHANGED",
    99: "FAMILY_CHART_RESULT",
    100: "FAMILY_INFO_RESULT",
    101: "FAMILY_RESULT",
    102: "FAMILY_JOIN_REQUEST",
    103: "FAMILY_JOIN_REQUEST_RESULT",
    104: "FAMILY_JOIN_ACCEPTED",
    105: "FAMILY_PRIVILEGE_LIST",
    106: "FAMILY_FAMOUS_POINT_INC_RESULT",
    107: "FAMILY_NOTIFY_LOGIN_OR_LOGOUT",
    108: "FAMILY_SET_PRIVILEGE",
    110: "NOTIFY_LEVELUP",
    111: "NOTIFY_MARRIAGE",
    112: "NOTIFY_JOB_CHANGE",
    114: "MAPLE_TV_USE_RES",
    115: "AVATAR_MEGAPHONE_RESULT",
    116: "SET_AVATAR_MEGAPHONE",
    117: "CLEAR_AVATAR_MEGAPHONE",
    118: "CANCEL_NAME_CHANGE_RESULT",
    119: "CANCEL_TRANSFER_WORLD_RESULT",
    120: "DESTROY_SHOP_RESULT",
    121: "FAKE_GM_NOTICE",
    122: "SUCCESS_IN_USE_GACHAPON_BOX",
    123: "NEW_YEAR_CARD_RES",
    124: "RANDOM_MORPH_RES",
    125: "CANCEL_NAME_CHANGE_BY_OTHER",
    126: "SET_BUY_EQUIP_EXT",
    127: "SET_PASSENGER_REQUEST",
    128: "SCRIPT_PROGRESS_MESSAGE",
    129: "DATA_CRC_CHECK_FAILED",
    130: "CAKE_PIE_EVENT_RESULT",
    131: "UPDATE_GM_BOARD",
    132: "SHOW_SLOT_MESSAGE",
    133: "ACCOUNT_MORE_INFO",
    134: "FIND_FRIEND",
    135: "STAGE_CHANGE",
    136: "DRAGON_BALL_BOX",
    137: "ASK_WHETHER_USE_PAMS_SONG",
    138: "TRANSFER_CHANNEL",
    139: "MACRO_SYS_DATA_INIT",
    140: "SET_FIELD",
    141: "SET_ITC",
    142: "SET_CASH_SHOP",
    143: "SET_BACK_EFFECT",
    144: "SET_MAP_OBJECT_VISIBLE",
    145: "CLEAR_BACK_EFFECT",
    146: "BLOCKED_MAP",
    147: "BLOCKED_SERVER",
    149: "MULTICHAT",
    150: "WHISPER",
    151: "SPOUSE_CHAT",
    152: "SUMMON_ITEM_INAVAILABLE",
    153: "FIELD_EFFECT",
    154: "FIELD_OBSTACLE_ONOFF",
    155: "FIELD_OBSTACLE_ONOFF_STATUS",
    156: "FIELD_OBSTACLE_ALL_RESET",
    157: "BLOW_WEATHER",
    158: "PLAY_JUKEBOX",
    159: "ADMIN_RESULT",
    160: "OX_QUIZ",
    161: "GMEVENT_INSTRUCTIONS",
    162: "CLOCK",
    163: "CONTI_MOVE",
    164: "CONTI_STATE",
    165: "SET_QUEST_CLEAR",
    166: "SET_QUEST_TIME",
    167: "WARN_MESSAGE",
    168: "SET_OBJECT_STATE",
    169: "STOP_CLOCK",
    170: "ARIANT_ARENA_SHOW_RESULT",
    171: "PYRAMID_GAUGE",
    172: "PYRAMID_SCORE",
    174: "QUICKSLOT_SET",
    177: "SPAWN_PLAYER",
    178: "REMOVE_PLAYER_FROM_MAP",
    179: "CHATTEXT",
    180: "CHATTEXT1",
    181: "CHALKBOARD",
    182: "UPDATE_CHAR_BOX",
    183: "SHOW_CONSUME_EFFECT",
    184: "SHOW_SCROLL_EFFECT",
    185: "SHOW_ITEM_HYPER_UPGRADE_EFFECT",
    186: "SHOW_ITEM_OPTION_UPGRADE_EFFECT",
    187: "SHOW_ITEM_RELEASE_EFFECT",
    188: "SHOW_ITEM_UNRELEASE_EFFECT",
    189: "HIT_BY_USER",
    190: "TESLA_TRIANGLE",
    191: "FOLLOW_CHARACTER",
    192: "SET_PHASE",
    195: "SPAWN_PET",
    196: "EVOLVE_PET",
    198: "MOVE_PET",
    199: "PET_CHAT",
    200: "PET_NAMECHANGE",
    201: "PET_EXCEPTION_LIST_RESULT",
    202: "PET_COMMAND",
    203: "SPAWN_SPECIAL_MAPOBJECT",
    204: "REMOVE_SPECIAL_MAPOBJECT",
    205: "MOVE_SUMMON",
    206: "SUMMON_ATTACK",
    207: "DAMAGE_SUMMON",
    208: "SUMMON_SKILL",
    209: "SPAWN_DRAGON",
    210: "MOVE_DRAGON",
    211: "REMOVE_DRAGON",
    213: "MOVE_PLAYER",
    214: "CLOSE_RANGE_ATTACK",
    215: "RANGED_ATTACK",
    216: "MAGIC_ATTACK",
    217: "ENERGY_ATTACK",
    218: "SKILL_EFFECT",
    219: "CANCEL_SKILL_EFFECT",
    220: "DAMAGE_PLAYER",
    221: "FACIAL_EXPRESSION",
    222: "SHOW_ITEM_EFFECT",
    223: "SHOW_UPGRADE_TOMB_EFFECT",
    224: "SHOW_CHAIR",
    225: "UPDATE_CHAR_LOOK",
    226: "SHOW_FOREIGN_EFFECT",
    227: "GIVE_FOREIGN_BUFF",
    228: "CANCEL_FOREIGN_BUFF",
    229: "UPDATE_PARTYMEMBER_HP",
    230: "GUILD_NAME_CHANGED",
    231: "GUILD_MARK_CHANGED",
    233: "CANCEL_CHAIR",
    235: "USER_EFFECT_LOCAL",
    236: "DOJO_WARP_UP",
    238: "LUCKSACK_PASS",
    239: "LUCKSACK_FAIL",
    240: "MESO_BAG_MESSAGE",
    244: "UPDATE_QUEST_INFO",
    247: "PLAYER_HINT",
    250: "MAKER_RESULT",
    252: "KOREAN_EVENT",
    253: "OPEN_UI",
    254: "OPEN_UI_WITH_OPTION",
    255: "LOCK_UI",
    256: "DISABLE_UI",
    257: "SPAWN_GUIDE",
    258: "TALK_GUIDE",
    259: "SHOW_COMBO",
    265: "NOTICE_MSG",
    266: "CHAT_MSG",
    267: "BUFFZONE_EFFECT",
    272: "FOLLOW_CHARACTER_FAILED",
    274: "COOLDOWN",
    276: "SPAWN_MONSTER",
    277: "KILL_MONSTER",
    278: "SPAWN_MONSTER_CONTROL",
    279: "MOVE_MONSTER",
    280: "MOVE_MONSTER_RESPONSE",
    282: "MobStatSet",
    283: "MobStatReset",
    284: "RESET_MONSTER_ANIMATION",
    285: "MOB_AFFECTED",
    286: "DAMAGE_MONSTER",
    287: "MONSTER_SPECIAL_EFFECT_BY_SKILL",
    289: "MOB_CRC_KEY_CHANGED",
    290: "SHOW_MONSTER_HP",
    291: "CATCH_MONSTER",
    292: "SHOW_MAGNET",
    294: "SHOW_RECOVERY_UPGRADE_COUNT_EFFECT",
    303: "SPAWN_NPC",
    304: "REMOVE_NPC",
    305: "SPAWN_NPC_REQUEST_CONTROLLER",
    306: "NPC_ACTION",
    307: "UPDATE_LIMITED_INFO",
    308: "NPC_SPECIAL_ACTION",
    309: "SET_NPC_SCRIPTABLE",
    311: "SPAWN_HIRED_MERCHANT",
    312: "DESTROY_HIRED_MERCHANT",
    313: "UPDATE_HIRED_MERCHANT",
    314: "DROP_ITEM_FROM_MAPOBJECT",
    316: "REMOVE_ITEM_FROM_MAP",
    317: "SPAWN_KITE_MESSAGE",
    318: "SPAWN_KITE",
    319: "DESTROY_KITE",
    320: "SPAWN_MIST",
    321: "REMOVE_MIST",
    322: "SPAWN_DOOR",
    323: "REMOVE_DOOR",
    324: "OPEN_GATE_CREATED",
    325: "OPEN_GATE_REMOVED",
    326: "REACTOR_HIT",
    327: "REACTOR_MOVE",
    328: "REACTOR_SPAWN",
    329: "REACTOR_DESTROY",
    330: "SNOWBALL_STATE",
    331: "HIT_SNOWBALL",
    332: "SNOWBALL_MESSAGE",
    333: "LEFT_KNOCK_BACK",
    334: "COCONUT_HIT",
    335: "COCONUT_SCORE",
    336: "GUILD_BOSS_HEALER_MOVE",
    337: "GUILD_BOSS_PULLEY_STATE_CHANGE",
    338: "MONSTER_CARNIVAL_START",
    339: "MONSTER_CARNIVAL_OBTAINED_CP",
    340: "MONSTER_CARNIVAL_PARTY_CP",
    341: "MONSTER_CARNIVAL_SUMMON",
    342: "MONSTER_CARNIVAL_MESSAGE",
    343: "MONSTER_CARNIVAL_DIED",
    344: "MONSTER_CARNIVAL_LEAVE",
    345: "MONSTER_CARNIVAL_RESULT",
    346: "ARIANT_ARENA_USER_SCORE",
    347: "SHEEP_RANCH_INFO",
    348: "SHEEP_RANCH_CLOTHES",
    349: "ARIANT_SCORE",
    350: "HORNTAIL_CAVE",
    351: "ZAKUM_SHRINE",
    355: "NPC_TALK",
    356: "OPEN_NPC_SHOP",
    357: "CONFIRM_SHOP_TRANSACTION",
    358: "ADMIN_SHOP_RESULT",
    359: "ADMIN_SHOP_COMMODITY",
    360: "STORAGE",
    361: "FREDRICK_MESSAGE",
    362: "FREDRICK",
    363: "RPS_GAME",
    364: "MESSENGER",
    365: "PLAYER_INTERACTION",
    366: "TOURNAMENT",
    367: "TOURNAMENT_MATCH_TABLE",
    368: "TOURNAMENT_SET_PRIZE",
    369: "TOURNAMENT_UEW",
    370: "TOURNAMENT_CHARACTERS",
    371: "WEDDING_PROGRESS",
    372: "WEDDING_CEREMONY_END",
    373: "PARCEL",
    374: "CHARGE_PARAM_RESULT",
    375: "QUERY_CASH_RESULT",
    376: "CASHSHOP_OPERATION",
    377: "CASH_PURCHASE_EXP_CHANGED",
    378: "GIFT_MATE_INFO_RESULT",
    379: "CHECK_DUPLICATED_ID_RESULT",
    380: "CHECK_NAME_CHANGE_POSSIBLE_RESULT",
    382: "CHECK_TRANSFER_WORLD_POSSIBLE_RESULT",
    383: "GACHAPON_STAMP_ITEM_RESULT",
    384: "CASH_ITEM_GACHAPON_RESULT",
    385: "CASH_GACHAPON_OPEN_RESULT",
    387: "ONE_A_DAY",
    389: "KEYMAP",
    390: "AUTO_HP_POT",
    391: "AUTO_MP_POT",
    396: "SEND_TV",
    397: "REMOVE_TV",
    398: "ENABLE_TV",
    399: "MTS_OPERATION2",
    400: "MTS_OPERATION",
    401: "ITC_NORMAL_ITEM_RESULT",
    412: "VICIOUS_HAMMER",
    416: "VEGA_RESULT",
    417: "VEGA_FAIL",
}

# Client->server opcodes from recvops-92.properties
SEND_OPCODES = {
    1: "LOGIN_PASSWORD",
    2: "GUEST_LOGIN",
    4: "SERVERLIST_REREQUEST",
    5: "CHARLIST_REQUEST",
    6: "SERVERSTATUS_REQUEST",
    7: "ACCEPT_TOS",
    8: "SET_GENDER",
    9: "AFTER_LOGIN",
    10: "REGISTER_PIN",
    11: "SERVERLIST_REQUEST",
    12: "PLAYER_DC",
    13: "VIEW_ALL_CHAR",
    14: "PICK_ALL_CHAR",
    19: "CHAR_SELECT",
    20: "PLAYER_LOGGEDIN",
    21: "CHECK_CHAR_NAME",
    22: "CREATE_CHAR",
    23: "CREATE_CHAR_IN_CS",
    24: "DELETE_CHAR",
    25: "PONG",
    26: "CLIENT_START_ERROR",
    27: "CLIENT_ERROR",
    28: "RELOG",
    30: "REGISTER_PIC",
    31: "CHAR_SELECT_WITH_PIC",
    32: "VIEW_ALL_PIC_REGISTER",
    33: "VIEW_ALL_WITH_PIC",
    36: "CLIENT_START",
    38: "PACKET_ERROR",
    43: "CHANGE_MAP",
    44: "CHANGE_CHANNEL",
    45: "ENTER_CASHSHOP",
    46: "MOVE_PLAYER",
    47: "CANCEL_CHAIR",
    48: "USE_CHAIR",
    49: "CLOSE_RANGE_ATTACK",
    50: "RANGED_ATTACK",
    51: "MAGIC_ATTACK",
    52: "TOUCH_MONSTER_ATTACK",
    53: "TAKE_DAMAGE",
    55: "GENERAL_CHAT",
    56: "CLOSE_CHALKBOARD",
    57: "FACE_EXPRESSION",
    58: "USE_ITEMEFFECT",
    59: "USE_DEATHITEM",
    63: "MONSTER_BOOK_COVER",
    64: "NPC_TALK",
    65: "REMOTE_STORE",
    66: "NPC_TALK_MORE",
    67: "NPC_SHOP",
    68: "STORAGE",
    69: "HIRED_MERCHANT_REQUEST",
    70: "FREDRICK_ACTION",
    71: "DUEY_ACTION",
    72: "USER_EFFECT_LOCAL",
    73: "OWL_OPEN",
    74: "OWL_WARP",
    75: "ADMIN_SHOP_REQUEST",
    76: "GATHER_ITEM",
    77: "SORT_ITEM",
    78: "ITEM_MOVE",
    79: "USE_ITEM",
    80: "CANCEL_ITEM_EFFECT",
    81: "STATE_CHANGE_BY_PORTABLE_CHAIR_REQUEST",
    82: "USE_SUMMON_BAG",
    83: "PET_FOOD",
    84: "USE_MOUNT_FOOD",
    85: "SCRIPTED_ITEM",
    86: "USE_CASH_ITEM",
    87: "DESTROY_PET_ITEM",
    88: "USE_CATCH_ITEM",
    89: "USE_SKILL_BOOK",
    90: "USE_SKILL_RESET_BOOK",
    91: "USE_TELEPORT_ROCK",
    92: "USE_RETURN_SCROLL",
    93: "USE_UPGRADE_SCROLL",
    94: "HYPER_UPGRADE_ITEM_USE",
    95: "ITEM_OPTION_UPGRADE_ITEM_USE",
    96: "ITEM_RELEASE_REQUEST",
    97: "DISTRIBUTE_AP",
    98: "AUTO_DISTRIBUTE_AP",
    99: "HEAL_OVER_TIME",
    101: "DISTRIBUTE_SP",
    102: "SPECIAL_MOVE",
    103: "CANCEL_BUFF",
    104: "SKILL_EFFECT",
    105: "MESO_DROP",
    106: "GIVE_FAME",
    108: "CHAR_INFO_REQUEST",
    109: "SPAWN_PET",
    110: "CANCEL_DEBUFF",
    111: "CHANGE_MAP_SPECIAL",
    112: "USE_INNER_PORTAL",
    113: "TROCK_ADD_MAP",
    116: "REPORT",
    118: "QUEST_ACTION",
    119: "USER_CALC_DAMAGE_STAT_SET_REQUEST",
    120: "THROW_GRENADE",
    121: "SKILL_MACRO",
    123: "USE_ITEM_REWARD",
    124: "MAKER_SKILL",
    127: "USE_REMOTE",
    128: "USE_WATER_OF_LIFE",
    133: "USER_FOLLOW_CHARACTER_REQUEST",
    134: "USER_FOLLOW_CHARACTER_WITHDRAW",
    135: "SET_PASSENSER_RESULT",
    136: "ADMIN_CHAT",
    137: "PARTYCHAT",
    138: "WHISPER",
    139: "SPOUSE_CHAT",
    140: "MESSENGER",
    141: "PLAYER_INTERACTION",
    142: "PARTY_REQUEST",
    143: "PARTY_RESULT",
    144: "EXPEDITION_REQUEST",
    146: "GUILD_OPERATION",
    147: "DENY_GUILD_REQUEST",
    148: "ADMIN_COMMAND",
    149: "ADMIN_LOG",
    150: "BUDDYLIST_MODIFY",
    151: "NOTE_ACTION",
    152: "NOTE_FLAG_REQUEST",
    153: "USE_DOOR",
    154: "OPEN_GATE",
    156: "CHANGE_KEYMAP",
    157: "RPS_ACTION",
    158: "RING_ACTION",
    161: "WEDDING_ACTION",
    162: "ITEM_VAC_ALERT",
    164: "ALLIANCE_REQUEST",
    165: "ALLIANCE_OPERATION",
    176: "BBS_OPERATION",
    177: "ENTER_MTS",
    178: "USE_SOLOMON_ITEM",
    179: "USE_GACHA_EXP",
    182: "CASH_ITEM_GACHAPON_REQUEST",
    185: "CLICK_GUIDE",
    186: "ARAN_COMBO_COUNTER",
    187: "MOB_CRC_KEY_CHANGED_REPLY",
    190: "ACCOUNT_MORE_INFO",
    191: "FIND_FRIEND",
    196: "MOVE_PET",
    197: "PET_CHAT",
    198: "PET_COMMAND",
    199: "PET_LOOT",
    200: "PET_AUTO_POT",
    201: "PET_EXCLUDE_ITEMS",
    204: "MOVE_SUMMON",
    205: "SUMMON_ATTACK",
    206: "DAMAGE_SUMMON",
    207: "BEHOLDER",
    211: "MOVE_DRAGON",
    213: "QUICKSLOT_CHANGE",
    220: "MOVE_LIFE",
    221: "AUTO_AGGRO",
    222: "MOB_DROP_PICKUP_REQUEST",
    223: "MOB_HIT_BY_OBSTACLE",
    224: "MOB_DAMAGE_MOB_FRIENDLY",
    225: "MOB_SELF_DESTRUCT",
    226: "MOB_DAMAGE_MOB",
    227: "MOB_SKILL_DELAY_END",
    228: "MOB_TIME_BOMB_END",
    229: "MOB_ESCORT_COLLISION",
    230: "MOB_REQUEST_ESCORT_INFO",
    231: "MOB_ESCORT_STOP_END_REQUEST",
    234: "NPC_ACTION",
    239: "ITEM_PICKUP",
    242: "DAMAGE_REACTOR",
    243: "TOUCHING_REACTOR",
    244: "REQUIRE_FIELD_OBSTACLE_STATUS",
    248: "SNOWBALL",
    249: "LEFT_KNOCKBACK",
    250: "COCONUT",
    251: "MATCH_TABLE",
    255: "MONSTER_CARNIVAL",
    258: "PARTY_SEARCH_REGISTER",
    259: "PARTY_SEARCH_START",
    260: "CANCEL_INVITE_PARTY_MATCH",
    267: "CHECK_CASH",
    268: "CASHSHOP_OPERATION",
    269: "COUPON_CODE",
    303: "LOGOUT_GIFT",
    65534: "MAPLETV",
    65535: "CRC_STATE_RESPONSE",
}

class MessageHeader(IntEnum):
    """Packet message types"""
    SENDPACKET = 0
    RECVPACKET = 1
    ENCODE_BEGIN = 2
    ENCODEHEADER = 3
    ENCODE1 = 4
    ENCODE2 = 5
    ENCODE4 = 6
    ENCODE8 = 7
    ENCODESTR = 8
    ENCODEBUFFER = 9
    TV_ENCODEHEADER = 10
    TV_ENCODESTRW1 = 11
    TV_ENCODESTRW2 = 12
    TV_ENCODEFLOAT = 13
    ENCODE_END = 14
    DECODE_BEGIN = 15
    DECODEHEADER = 16
    DECODE1 = 17
    DECODE2 = 18
    DECODE4 = 19
    DECODE8 = 20
    DECODESTR = 21
    DECODEBUFFER = 22
    TV_DECODEHEADER = 23
    TV_DECODESTRW1 = 24
    TV_DECODESTRW2 = 25
    TV_DECODEFLOAT = 26
    DECODE_END = 27
    UNKNOWNDATA = 28
    NOTUSED = 29
    WHEREFROM = 30
    UNKNOWN = 31


TCP_MESSAGE_MAGIC = 0xA11CE


class PacketMonitor:
    """TCP client for monitoring packets from RirePE DLL"""

    def __init__(self, host='127.0.0.1', port=9999):
        self.host = host
        self.port = port
        self.sock = None
        self.packet_count = 0
        self.log_file = None

    def connect(self):
        """Connect to the DLL's TCP server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            print(f"[+] Connected to {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"[-] Connection failed: {e}")
            return False

    def disconnect(self):
        """Disconnect from server"""
        if self.sock:
            self.sock.close()
            self.sock = None
            print("[+] Disconnected")

    def recv_message(self):
        """Receive a message from the DLL (with magic + length header)"""
        try:
            # Read magic (4 bytes)
            magic_data = self._recv_exact(4)
            if not magic_data:
                return None

            magic = struct.unpack('<I', magic_data)[0]
            if magic != TCP_MESSAGE_MAGIC:
                print(f"[-] Invalid magic: 0x{magic:08X}")
                return None

            # Read length (4 bytes)
            length_data = self._recv_exact(4)
            if not length_data:
                return None

            length = struct.unpack('<I', length_data)[0]
            if length == 0 or length > 1024 * 1024:  # 1MB max
                print(f"[-] Invalid length: {length}")
                return None

            # Read data
            data = self._recv_exact(length)
            return data

        except Exception as e:
            print(f"[-] Receive error: {e}")
            return None

    def send_message(self, data):
        """Send a message to the DLL (with magic + length header)"""
        try:
            length = len(data)
            # Pack: magic (4 bytes) + length (4 bytes) + data
            message = struct.pack('<II', TCP_MESSAGE_MAGIC, length) + data
            self.sock.sendall(message)
            return True
        except Exception as e:
            print(f"[-] Send error: {e}")
            return False

    def _recv_exact(self, n):
        """Receive exactly n bytes from socket"""
        data = b''
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                return None
            data += chunk
        return data

    def parse_packet_message(self, data):
        """Parse PacketEditorMessage structure"""
        if len(data) < 16:  # Minimum size
            return None

        # Parse header (assuming 64-bit build for now)
        # struct: header (4) + id (4) + addr (8)
        header, packet_id, addr = struct.unpack('<IIQ', data[:16])

        try:
            header_name = MessageHeader(header).name
        except ValueError:
            header_name = f"UNKNOWN_{header}"

        result = {
            'header': header,
            'header_name': header_name,
            'id': packet_id,
            'addr': addr,
            'data': data[16:]
        }

        # Parse based on message type
        if header in (MessageHeader.SENDPACKET, MessageHeader.RECVPACKET):
            if len(data) >= 20:
                pkt_length = struct.unpack('<I', data[16:20])[0]
                packet_data = data[20:20+pkt_length]
                result['packet_length'] = pkt_length
                result['packet_data'] = packet_data

        return result

    def format_hex(self, data, max_bytes=32):
        """Format binary data as hex string"""
        hex_str = ' '.join(f'{b:02X}' for b in data[:max_bytes])
        if len(data) > max_bytes:
            hex_str += '...'
        return hex_str

    def log_packet(self, msg):
        """Log a packet message to file"""
        # Don't log DECODE_END packets
        if msg['header'] == MessageHeader.DECODE_END:
            return

        self.packet_count += 1
        timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]

        direction = '>>>' if msg['header'] == MessageHeader.SENDPACKET else '<<<'

        log_line = f"\n[{self.packet_count}] {timestamp} {direction} {msg['header_name']}\n"

        # For SENDPACKET and RECVPACKET, use modified format
        if msg['header'] in (MessageHeader.SENDPACKET, MessageHeader.RECVPACKET):
            if 'packet_data' in msg and len(msg['packet_data']) >= 2:
                # Extract first 2 bytes (4 hex digits): LL (left byte) and RR (right byte)
                first_2_bytes = msg['packet_data'][:2]
                LL = first_2_bytes[0]  # Left byte
                RR = first_2_bytes[1]  # Right byte
                header_value = f"0x{RR:02X}{LL:02X}"

                # Convert header to decimal and look up opcode name
                opcode_decimal = (RR << 8) | LL
                opcode_dict = SEND_OPCODES if msg['header'] == MessageHeader.SENDPACKET else RECV_OPCODES
                opcode_name = opcode_dict.get(opcode_decimal, "UNKNOWN")

                log_line += f"  Header: {header_value} ({opcode_name})\n"
                # Write full data without truncating
                log_line += f"  Data: {msg['packet_data'].hex()}\n"
            else:
                # If data is less than 2 bytes, show what we have
                log_line += f"  Header: (insufficient data)\n"
                if 'packet_data' in msg:
                    log_line += f"  Data: {msg['packet_data'].hex()}\n"
        else:
            # For other message types, keep original format
            log_line += f"  ID: {msg['id']}, Addr: 0x{msg['addr']:016X}\n"
            if 'packet_data' in msg:
                log_line += f"  Length: {msg['packet_length']}\n"
                log_line += f"  Data: {self.format_hex(msg['packet_data'])}\n"
                log_line += f"  Hex: {msg['packet_data'].hex()}\n"

        if self.log_file:
            self.log_file.write(log_line)
            self.log_file.flush()

    def send_packet_to_dll(self, packet_data, is_recv=False):
        """Send a packet to the DLL for injection"""
        # Build PacketEditorMessage
        header = MessageHeader.RECVPACKET if is_recv else MessageHeader.SENDPACKET
        packet_id = 9999  # Custom ID
        addr = 0  # No return address

        # Pack header
        msg_header = struct.pack('<IIQ', header, packet_id, addr)

        # Pack binary data (length + packet)
        msg_data = struct.pack('<I', len(packet_data)) + packet_data

        # Send complete message
        full_message = msg_header + msg_data
        if self.send_message(full_message):
            # Wait for response
            response_data = self.recv_message()
            if response_data and len(response_data) >= 1:
                blocked = response_data[0]
                print(f"[+] Packet {'blocked' if blocked else 'allowed'}")
                return True

        return False

    def run(self, log_file=None):
        """Main monitoring loop"""
        # Create timestamped log file if not provided
        if not log_file:
            timestamp = datetime.now().strftime('%Y-%m-%dT%H:%M:%S')
            log_file = f"packet-monitor-{timestamp}.log"

        self.log_file = open(log_file, 'w')
        print(f"[+] Logging to {log_file}")

        try:
            print("[+] Monitoring packets (Ctrl+C to stop)...")
            while True:
                data = self.recv_message()
                if not data:
                    print("[-] Connection closed")
                    break

                msg = self.parse_packet_message(data)
                if msg:
                    self.log_packet(msg)

        except KeyboardInterrupt:
            print("\n[+] Stopped by user")
        finally:
            if self.log_file:
                self.log_file.close()
                print(f"[+] Log saved to {log_file}")


def main():
    parser = argparse.ArgumentParser(description='RirePE Packet Monitor - TCP Client')
    parser.add_argument('--host', default='127.0.0.1', help='Server host (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=9999, help='Server port (default: 9999)')
    parser.add_argument('--log', help='Log file path')
    parser.add_argument('--send', help='Send a hex packet (e.g., "0A 00 01 02 03")')
    parser.add_argument('--send-recv', action='store_true', help='Send as recv packet (default: send)')

    args = parser.parse_args()

    monitor = PacketMonitor(args.host, args.port)

    if not monitor.connect():
        return 1

    try:
        if args.send:
            # Send mode
            hex_str = args.send.replace(' ', '').replace('0x', '')
            packet_data = bytes.fromhex(hex_str)
            print(f"[+] Sending packet: {monitor.format_hex(packet_data)}")
            monitor.send_packet_to_dll(packet_data, args.send_recv)
        else:
            # Monitor mode
            monitor.run(args.log)
    finally:
        monitor.disconnect()

    return 0


if __name__ == '__main__':
    sys.exit(main())
