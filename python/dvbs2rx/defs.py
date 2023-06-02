standards = {
    'DVB-T2': 'STANDARD_DVBT2',
    'DVB-S2': 'STANDARD_DVBS2',
    'DVB-S2X': 'STANDARD_DVBS2'  # mapped as S2
}

frame_sizes = {
    'normal': 'FECFRAME_NORMAL',
    'medium': 'FECFRAME_MEDIUM',
    'short': 'FECFRAME_SHORT'
}

constellations = {
    'QPSK': {
        'def': 'MOD_QPSK',
        'standard': ['DVB-T2', 'DVB-S2']
    },
    '16QAM': {
        'def': 'MOD_16QAM',
        'standard': ['DVB-T2']
    },
    '64QAM': {
        'def': 'MOD_64QAM',
        'standard': ['DVB-T2']
    },
    '256QAM': {
        'def': 'MOD_256QAM',
        'standard': ['DVB-T2']
    },
    '8PSK': {
        'def': 'MOD_8PSK',
        'standard': ['DVB-S2']
    }
}

rolloffs = {
    0.35: {
        'def': 'RO_0_35',
        'standard': ['DVB-S2', 'DVB-S2X']
    },
    0.25: {
        'def': 'RO_0_25',
        'standard': ['DVB-S2', 'DVB-S2X']
    },
    0.2: {
        'def': 'RO_0_20',
        'standard': ['DVB-S2', 'DVB-S2X']
    },
    0.15: {
        'def': 'RO_0_15',
        'standard': ['DVB-S2X']
    },
    0.1: {
        'def': 'RO_0_10',
        'standard': ['DVB-S2X']
    },
    0.05: {
        'def': 'RO_0_05',
        'standard': ['DVB-S2X']
    },
}

pilots = {False: "PILOTS_OFF", True: "PILOTS_ON"}

ldpc_codes = {
    '1/4': {
        'def': 'C1_4',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2', 'DVB-T2']
    },
    '1/3': {
        'def': ['C1_3', 'C1_3', 'C1_3_MEDIUM', 'C1_3_VLSNR'],
        'frame': ['normal', 'short', 'medium', 'short'],
        'standard': ['DVB-S2X', 'DVB-S2', 'DVB-T2'],
        'vl-snr': [False, False, True, True]
    },
    '2/5': {
        'def': 'C2_5',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2', 'DVB-T2']
    },
    '1/2': {
        'def': 'C1_2',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2', 'DVB-T2']
    },
    '3/5': {
        'def': 'C3_5',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2', 'DVB-T2']
    },
    '2/3': {
        'def': 'C2_3',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2', 'DVB-T2']
    },
    '3/4': {
        'def': 'C3_4',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2', 'DVB-T2']
    },
    '4/5': {
        'def': 'C4_5',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2', 'DVB-T2']
    },
    '5/6': {
        'def': 'C5_6',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2', 'DVB-T2']
    },
    '8/9': {
        'def': 'C8_9',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2']
    },
    '9/10': {
        'def': 'C9_10',
        'frame': ['normal'],
        'standard': ['DVB-S2']
    },
    '2/9': {
        'def': 'C2_9_VLSNR',
        'frame': ['normal'],
        'standard': ['DVB-S2X'],
        'vl-snr': True
    },
    '13/45': {
        'def': 'C13_45',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '9/20': {
        'def': 'C9_20',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '90/180': {
        'def': 'C90_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '96/180': {
        'def': 'C96_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '11/20': {
        'def': 'C11_20',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '100/180': {
        'def': 'C100_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '104/180': {
        'def': 'C104_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '26/45': {
        'def': 'C26_45',
        'frame': ['normal', 'short'],
        'standard': ['DVB-S2X']
    },
    '18/30': {
        'def': 'C18_30',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '28/45': {
        'def': 'C28_45',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '23/36': {
        'def': 'C23_36',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '116/180': {
        'def': 'C116_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '20/30': {
        'def': 'C20_30',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '124/180': {
        'def': 'C124_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '25/36': {
        'def': '25/36',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '128/180': {
        'def': 'C128_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '13/18': {
        'def': 'C13_18',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '132/180': {
        'def': 'C132_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '22/30': {
        'def': 'C22_30',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '135/180': {
        'def': 'C135_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '140/180': {
        'def': 'C140_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '7/9': {
        'def': 'C7_9',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '154/180': {
        'def': 'C154_180',
        'frame': ['normal'],
        'standard': ['DVB-S2X']
    },
    '1/5': {
        'def': ['C1_5_MEDIUM', 'C1_5_VLSNR_SF2', 'C1_5_VLSNR'],
        'frame': ['medium', 'short', 'short'],
        'standard': ['DVB-S2X'],
        'vl-snr': True
    },
    '11/45': {
        'def': ['C11_45', 'C11_45_MEDIUM', 'C11_45_VLSNR_SF2'],
        'frame': ['short', 'medium', 'short'],
        'standard': ['DVB-S2X'],
        'vl-snr': [False, True, True]
    },
    '4/15': {
        'def': ['C4_15', 'C4_15_VLSNR'],
        'frame': ['short', 'short'],
        'standard': ['DVB-S2X'],
        'vl-snr': [False, True]
    },
    '14/45': {
        'def': 'C14_45',
        'frame': ['short'],
        'standard': ['DVB-S2X']
    },
    '7/15': {
        'def': 'C7_15',
        'frame': ['short'],
        'standard': ['DVB-S2X']
    },
    '8/15': {
        'def': 'C8_15',
        'frame': ['short'],
        'standard': ['DVB-S2X']
    },
    '32/45': {
        'def': 'C32_45',
        'frame': ['short'],
        'standard': ['DVB-S2X']
    }
}

dvbs2_modcods = {
    'qpsk1/4': 1,
    'qpsk1/3': 2,
    'qpsk2/5': 3,
    'qpsk1/2': 4,
    'qpsk3/5': 5,
    'qpsk2/3': 6,
    'qpsk3/4': 7,
    'qpsk4/5': 8,
    'qpsk5/6': 9,
    'qpsk8/9': 10,
    'qpsk9/10': 11,
    '8psk3/5': 12,
    '8psk2/3': 13,
    '8psk3/4': 14,
    '8psk5/6': 15,
    '8psk8/9': 16,
    '8psk9/10': 17,
    '16apsk2/3': 18,
    '16apsk3/4': 19,
    '16apsk4/5': 20,
    '16apsk5/6': 21,
    '16apsk8/9': 22,
    '16apsk9/10': 23,
    '32apsk3/4': 24,
    '32apsk4/5': 25,
    '32apsk5/6': 26,
    '32apsk8/9': 27,
    '32apsk9/10': 28
}
