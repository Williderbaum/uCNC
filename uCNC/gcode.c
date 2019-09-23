#include "gcode.h"
#include "config.h"
#include "report.h"
#include "error.h"
#include "utils.h"
#include "board.h"
#include <math.h>
#include <string.h>

#define MUL10(X) (((X<<2) + X)<<1)

static GCODE_PARSER_STATE g_gcparser_state;

/*
	Parses a string to number (real)
	If the number is an integer the isinteger flag is set
	The string pointer is also advanced to the next position
*/

bool gcode_parse_float(char **str, float *value, bool *isinteger)
{
    bool isnegative = false;
    bool isfloat = false;
    uint32_t intval = 0;
    uint8_t fpcount = 0;
    bool result = false;

    if (**str == '-')
    {
        isnegative = true;
        (*str)++;
    }
    else if (**str == '+')
    {
        (*str)++;
    }

    for(;;)
    {
        uint8_t digit = (uint8_t)(**str - '0');
        if (digit <= 9)
        {
            intval = MUL10(intval) + digit;
            if (isfloat)
            {
                fpcount++;
            }

            result = true;
        }
        else if (**str == '.' && !isfloat)
        {
            isfloat = true;
        }
        else
        {
            break;
        }

        (*str)++;
        result = true;
    }
    
    *value = intval;
 
    do
    {
        if(fpcount>=3)
        {
            *value *= 0.001f;
            fpcount -= 3;
        }

        if(fpcount>=2)
        {
            *value *= 0.01f;
            fpcount -= 2;
        }

        if(fpcount>=1)
        {
            *value *= 0.1f;
            fpcount -= 1;
        }

    } while (fpcount !=0 );
    

    *isinteger = !isfloat;
    
    if(isnegative)
    {
    	*value = -*value;
	}
	
    return result;

}

/*
	parses comments as defined in the RS274NGC
	Suports nested comments
	If comment is not closed returns an error
*/

void gcode_parse_comment()
{
	uint8_t comment_nest = 1;
	for(;;)
	{
		while(board_peek()=='\0');
		
		char c = board_peek();
		switch(c)
		{
			case '(':
				comment_nest++;
				break;
			case ')':
				comment_nest--;
				if(comment_nest == 0)
				{
					board_getc();
					return;
				}
				break;
			case '\n':
			case '\r':
				report_error(GCODE_INVALID_COMMENT);
                return;
		}
		
		board_getc();
	}	
}

/*
	STEP 1
	Fetches the next line from the board communication buffer and preprocesses the string
	In the preprocess these steps are executed
		1. Whitespaces are removed
		2. Comments are parsed (nothing is done besides parsing for now)
		3. All letters are changed to upper-case	
*/

void gcode_fetch_frombuffer(char *str)
{
	uint8_t count = 0;
	if(board_peek() != 0)
	{
		for(;;)
		{
			while(board_peek()==0);
			
			char c = board_getc();	
			switch(c)
			{
				case ' ':
				case '\t':
					//ignore whitechars
					break;
				case '(':
					gcode_parse_comment();
					puts("comment");
					break;
				case '\n':
				case '\r':
					*str = '\0';
					return;
				default:
					if(c>='a' && c<='z')
					{
						c -= 32;
					}
					*str = c;
					str++;
					break;
			}
		}
	}
}

/*
	STEP 2
	Parse the hole string and updates the values of the parser new state
    In this step the parser will check if:
    	1. There is a valid word character followed by a number
    	2. If there is no modal groups or word repeating violations
    	3. For words N, M, T and L check if the value is an integer
    	4. If N is in the beginning of the line
    	
    After that the parser has to perform the following checks:
    	1. At least a G or M command must exist in a line
    	2. Words F, N, R and S must be positive (H, P, Q and T not implemented)
    	3. Motion codes must have at least one axis declared
*/

void gcode_parse_line(char* str, GCODE_PARSER_STATE *new_state)
{
    float word_val = 0.0;
    char word = '\0';
    uint8_t code = 0;
    uint8_t subcode = 0;
    uint8_t wordcount = 0;
    uint8_t mwords = 0;
    uint8_t axis_mask = 0x3F;

    //flags optimized for 8 bits CPU
    uint8_t group0 = 0;
    uint8_t group1 = 0;
    uint8_t word0 = 0;
    uint8_t word1 = 0;
    uint8_t word2 = 0;
    
    for(;;)
    {
        word = *str;
        if(word == '\0')
        {
            break;
        }

        str++;
        bool isinteger = false;
        if(!gcode_parse_float(&str, &word_val, &isinteger))
        {
            report_error(GCODE_BAD_NUMBER_FORMAT);
            return ;
        }

        switch(word)
        {
            case 'G':
                code = (uint8_t)(word_val);
                switch(code)
                {
                    //motion codes
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                        if(CHECKBIT(group0,GCODE_GROUP_MOTION))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_MOTION);
                        new_state->groups.motion = code;
                        break;
                    //unsuported
                    case 38://check if 38.2
                        if(CHECKBIT(group0,GCODE_GROUP_MOTION))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_MOTION);
                        subcode = (uint8_t)round((word_val - code) * 100.0f);
                        if(subcode == 20)
                        {
                            report_error(GCODE_UNSUPORTED_COMMAND);
                        }      
                        else
                        {
                            report_error(GCODE_UNKNOWN_GCOMMAND);
                        }
                        return;
                    case 80:
                    case 81:
                    case 82:
                    case 83:
                    case 84:
                    case 85:
                    case 86:
                    case 87:
                    case 88:
                    case 89:
                        if(CHECKBIT(group0,GCODE_GROUP_MOTION))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_MOTION);
                        code -= 75;
                        new_state->groups.motion = code;
                        break;
                    case 17:
                    case 18:
                    case 19:
                        if(CHECKBIT(group0,GCODE_GROUP_PLANE))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_PLANE);
                        code -= 17;
                        new_state->groups.plane = code;
                        break;
                    case 90:
                    case 91:
                        if(CHECKBIT(group0,GCODE_GROUP_DISTANCE))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_DISTANCE);
                        code -= 90;
                        new_state->groups.distance_mode = code;
                        break;
                    case 93:
                    case 94:
                        if(CHECKBIT(group0,GCODE_GROUP_FEEDRATE))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_FEEDRATE);
                        code -= 93;
                        new_state->groups.feedrate_mode = code;
                        break;
                    case 20:
                    case 21:
                        if(CHECKBIT(group0,GCODE_GROUP_UNITS))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_UNITS);  
                        code -= 20;
                        new_state->groups.units = code;
                        break;
                    case 40:
                    case 41:
                    case 42:
                        if(CHECKBIT(group0,GCODE_GROUP_CUTTERRAD))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_CUTTERRAD);
                        code -= 40;
                        new_state->groups.cutter_radius_compensation = code;
                        break;
                    case 43:
                        if(CHECKBIT(group0,GCODE_GROUP_TOOLLENGTH))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_TOOLLENGTH);
                        new_state->groups.tool_length_offset = 0;
                        break;
                    case 49:
                        if(CHECKBIT(group0,GCODE_GROUP_TOOLLENGTH))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_TOOLLENGTH);
                        new_state->groups.tool_length_offset = 1;
                        break;
                    case 98:
                    case 99:
                        if(CHECKBIT(group0,GCODE_GROUP_RETURNMODE))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group0,GCODE_GROUP_RETURNMODE);
                        code -= 98;
                        new_state->groups.return_mode = code;
                        break;
                    case 54:
                    case 55:
                    case 56:
                    case 57:
                    case 58:
                    case 59:
                        if(CHECKBIT(group1,GCODE_GROUP_COORDSYS))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group1,GCODE_GROUP_COORDSYS);
                        //59.X unsupported
                        if(code == 59)
                        {
                            subcode = (uint8_t)round((word_val - code) * 100.0f);
                            switch(subcode)
                            {
                                case 10:
                                case 20:
                                case 30:
                                    report_error(GCODE_UNSUPORTED_COMMAND);
                                    return;
                                default:
                                    report_error(GCODE_UNKNOWN_GCOMMAND);
                                    return;
                            }
                        }
                        code -= 54;
                        new_state->groups.coord_system = code;
                        break;
                    case 61:
                        if(CHECKBIT(group1,GCODE_GROUP_PATH))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group1,GCODE_GROUP_PATH);
                        new_state->groups.path_mode = 0;
                        break;
                    case 64:
                        if(CHECKBIT(group1,GCODE_GROUP_PATH))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group1,GCODE_GROUP_PATH);
                        new_state->groups.path_mode = 1;
                        break;
                    case 4:
                    case 10:
                    case 28:
                    case 30:
                    case 53:
                    case 92:
                        //convert code within 4 bits without 
                        //4 = 4
                        //10 = 1
                        //28 = 2
                        //30 = 3
                        //53 = 4
                        //92 = 9
                        if(code >= 10)
                        {
                            code /= 10;
                        }

                        if(CHECKBIT(group1,GCODE_GROUP_NONMODAL))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group1,GCODE_GROUP_NONMODAL);
                        new_state->groups.nonmodal = code;
                        break;
                    
                    default:
                        report_error(GCODE_UNKNOWN_GCOMMAND);
                        return ;
                }
                break;

            case 'M':
            	if(!isinteger)
	            {
	                report_error(GCODE_UNKNOWN_MCOMMAND);
	                return;
	            }
	            
	            //counts number of M commands
	            mwords++;
                code = (uint8_t)(word_val);
                switch(code)
                {
                    case 0:
                    case 1:
                    case 2:
                    case 30:
                    case 60:
                        if(CHECKBIT(group1,GCODE_GROUP_STOPPING))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group1,GCODE_GROUP_STOPPING);
                        if(code >= 10)
                        {
                            code /= 10;
                        }
                        new_state->groups.stopping = code;
                        break;
                    case 3:
                    case 4:
                    case 5:
                        if(CHECKBIT(group1,GCODE_GROUP_SPINDLE))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group1,GCODE_GROUP_SPINDLE);
                        code -= 3;
                        new_state->groups.spindle_turning = code;
                        break;
                    case 7:
                        new_state->groups.coolant |= 1;
                        break;
                    case 8:
                        new_state->groups.coolant |= 2;
                        break;
                    case 9:
                        new_state->groups.coolant = 0;
                        break;
                    case 48:
                    case 49:
                        if(CHECKBIT(group1,GCODE_GROUP_ENABLEOVER))
                        {
                            report_error(GCODE_MODAL_GROUP_VIOLATION);
                            return;
                        }

                        SETBIT(group1,GCODE_GROUP_ENABLEOVER);
                        code -= 48;
                        new_state->groups.feed_speed_override = code;
                        break;
                    default:
                    	report_error(GCODE_UNKNOWN_MCOMMAND);
	                	return;
                }
            break;
        case 'N':
            if(CHECKBIT(word2, GCODE_WORD_N))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            if(!isinteger || wordcount!=0)
            {
                report_error(GCODE_INVALID_LINE_NUMBER);
                return;
            }

            new_state->linenum = trunc(word_val);
        case 'X':
            if(CHECKBIT(word0, GCODE_WORD_X))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word0, GCODE_WORD_X);
            new_state->words.xyzabc[0] = word_val;
            break;
        case 'Y':
            if(CHECKBIT(word0, GCODE_WORD_Y))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word0, GCODE_WORD_Y);
            new_state->words.xyzabc[1] = word_val;
            break;
        case 'Z':
            if(CHECKBIT(word0, GCODE_WORD_Z))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word0, GCODE_WORD_Z);
            new_state->words.xyzabc[2] = word_val;
            break;
        case 'A':
            if(CHECKBIT(word0, GCODE_WORD_A))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word0, GCODE_WORD_A);
            new_state->words.xyzabc[3] = word_val;
            break;
        case 'B':
            if(CHECKBIT(word0, GCODE_WORD_B))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word0, GCODE_WORD_B);
            new_state->words.xyzabc[4] = word_val;
            break;
        case 'C':
            if(CHECKBIT(word0, GCODE_WORD_C))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word0, GCODE_WORD_C);
            new_state->words.xyzabc[5] = word_val;
            break;
        case 'D':
            if(CHECKBIT(word0, GCODE_WORD_D))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word0, GCODE_WORD_D);
            new_state->words.d = word_val;
            break;
        case 'F':
            if(CHECKBIT(word0, GCODE_WORD_F))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word0, GCODE_WORD_F);
            new_state->words.f = word_val;
            break;
        case 'H':
            if(CHECKBIT(word1, GCODE_WORD_H))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word1, GCODE_WORD_H);
            new_state->words.h = word_val;
            break;
        case 'I':
            if(CHECKBIT(word1, GCODE_WORD_I))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word1, GCODE_WORD_I);
            new_state->words.ijk[0] = word_val;
            break;
        case 'J':
            if(CHECKBIT(word1, GCODE_WORD_J))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word1, GCODE_WORD_J);
            new_state->words.ijk[1] = word_val;
            break;
        case 'K':
            if(CHECKBIT(word1, GCODE_WORD_K))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word1, GCODE_WORD_K);
            new_state->words.ijk[2] = word_val;
            break;
        case 'L':
            if(CHECKBIT(word1, GCODE_WORD_L))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }
            
            if(!isinteger)
            {
                report_error(GCODE_VALUE_NOT_INTEGER);
                return;
            }

            SETBIT(word1, GCODE_WORD_L);
            new_state->words.l= word_val;
            break;
        case 'P':
            if(CHECKBIT(word1, GCODE_WORD_P))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word1, GCODE_WORD_P);
            new_state->words.p = word_val;
            break;
        case 'Q':
            if(CHECKBIT(word1, GCODE_WORD_Q))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word1, GCODE_WORD_Q);
            new_state->words.q = word_val;
            break;
        case 'R':
            if(CHECKBIT(word1, GCODE_WORD_R))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word1, GCODE_WORD_R);
            new_state->words.r = word_val;
            break;
        case 'S':
            if(CHECKBIT(word2, GCODE_WORD_S))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }

            SETBIT(word2, GCODE_WORD_S);
            new_state->words.s = word_val;
            break;
        case 'T':
            if(CHECKBIT(word2, GCODE_WORD_T))
            {
                report_error(GCODE_WORD_REPEATED);
                return;
            }
            
            if(!isinteger)
            {
                report_error(GCODE_VALUE_NOT_INTEGER);
                return;
            }

            SETBIT(word2, GCODE_WORD_T);
            new_state->words.t = word_val;
            break;
        default:
            break;

        }
        wordcount++;
    }
    
    //The string is parsed
    //Starts to validate the string parameters
    
    //At least a G or M command must exist in a line
	if(group0==0 && group1 == 0)
    {
        report_error(GCODE_MISSING_COMMAND);
        return;
    }
    
    //Line number must be positive
    if(new_state->linenum<0)
    {
    	report_error(GCODE_INVALID_LINE_NUMBER);
        return;
	}
	
	//Words F, R and S must be positive
	//Words H, P, Q, and T are not implemented
	if(new_state->words.f<0.0f || new_state->words.r<0.0f|| new_state->words.s<0.0f)
    {
    	report_error(GCODE_VALUE_IS_NEGATIVE);
        return;
	}
	
	//check if axis are definined in motion commands
	if(new_state->groups.motion == 2 || new_state->groups.motion == 3)
	{
		switch(new_state->groups.plane)
		{
			case 0: //XY
				axis_mask = 0x02;
				break;
			case 1: //XZ
				axis_mask = 0x05;
				break;
			case 2: //YZ
				axis_mask = 0x06;
				break;
			default:
				axis_mask = 0x3F;
				break;
		}
	}
	
	//G0, G1, G2 and G3
	if(new_state->groups.motion <= 3)
	{
		if((word0 & axis_mask) == 0)
		{
			report_error(GCODE_UNDEFINED_AXIS);
        	return;
		}
	}
	
	//future
	//check if T is negative and smaller than max tool slots 
}

/*
	STEP 3
	In this step the parser do all remaining checks and send motion for the controller
*/

void gcode_execute_line(GCODE_PARSER_STATE *new_state)
{
	switch(new_state->groups.motion)
	{
		case 0:
		case 1:
			
			break;
	}
}

/*
	Initializes the gcode parser 
*/


void gcode_init()
{
	memset(&g_gcparser_state, 0, sizeof(GCODE_PARSER_STATE));
}

/*
	Parse the next gcode line available in the buffer and send it to the motion controller
*/

void gcode_parse_nextline()
{
	char gcode_line[GCODE_PARSER_BUFFER_SIZE];
	GCODE_PARSER_STATE next_state = {};

	//next state will be the same as previous except for nonmodal group (is set with 0)
	memcpy(&next_state, &g_gcparser_state, sizeof(GCODE_PARSER_STATE));
    next_state.groups.nonmodal = 0;
	
	gcode_fetch_frombuffer(&gcode_line[0]);
	#ifdef DEBUGMODE
		board_putc("fetched: ");
		board_putc(gcode_line);
	#endif
	gcode_parse_line(&gcode_line[0], &next_state);
	gcode_execute_line(&next_state);
}
