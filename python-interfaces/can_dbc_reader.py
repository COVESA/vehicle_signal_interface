import re
# import pprint
import ast

def get_can(dbc_file_path='utf8_can_dbc.txt'):
    dbc_file = open(dbc_file_path, 'r+')

    # print('Opened file:'+dbc_file.name)

    can_db = {}

    Flag = False

    # The can_db dictionary will be basically our candb lookup table for all
    # necessary information The structure of the beast will look something
    # like this:
    #
    # can_table = {
    #   <int arbitration_id>: {
    #       'family': 'blah',
    #       'genus': 'blah',
    #       'species': {
    #                   <str>: {
    #                       'length': <int>
    #                       'end_bit':<int>
    #                       'factor':<int or float>
    #                       'offset':<int or float>
    #                       'minimum':<int or float>
    #                       'maximum':<int or float>
    #                       'description':<str>
    #                       'current_value':<None or int>
    #                       'value':<"null" or whatever it actually is>
    #                   },
    #                   <str>: {
    #                       'length': <int>
    #                       'end_bit':<int>
    #                       'factor':<int or float>
    #                       'offset':<int or float>
    #                       'minimum':<int or float>
    #                       'maximum':<int or float>
    #                       'description':<str>
    #                       'current_value':<None or int>
    #                       'value':<"null" or whatever it actually is>
    #                   },
    #                   <...>
    #       }
    #   }
    #   <int arbitration_id>:{
    #       <...>
    #   }
    # }

    for line in dbc_file:
        line_array = line.split()
        if len(line_array) == 0:
            Flag = False
            current_BO = None
            continue
        elif Flag and line_array[0] == 'SG_':
            split_range = (line_array[3].replace('@','|')).split('|')

            factor_offset = (line_array[4].replace('(','').replace(')','')).split(",")

            value_range = (line_array[5].replace('[','').replace(']','')).split("|")

            can_db[current_BO]['species'][line_array[1]] =
            {
                'end_bit':int(split_range[0]), 'length':int(split_range[1]),
                # 'start_bit':(((int(split_range[0])-int(split_range[1])))+1),
                'factor':ast.literal_eval(factor_offset[0]),
                'offset':ast.literal_eval(factor_offset[1]),
                'minimum':ast.literal_eval(value_range[0]),
                'maximum':ast.literal_eval(value_range[1]),
                'description':line_array[6],
                'value': None
            }
        elif line_array[0] == 'BO_':
            #family is something like HVAC, FCIM, BCM ...
            #genus is the HVAC_A, HVAC_B, HVAC_C ...
            #species is the signal names contained within the frame
            can_db[int(line_array[1])] =
            {
                'family':line_array[4], 'genus':line_array[2][:-1], 'species':{},
                'frame_bytes':int(line_array[3])
            }
            Flag = True
            current_BO = int(line_array[1])
        else:
            pass

    dbc_file.close()

    return can_db

if __name__ == '__main__':
    import pprint

    pp = pprint.PrettyPrinter(indent=4)
    pp.pprint(get_can())
