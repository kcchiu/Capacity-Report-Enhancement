#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIBell3S1PATL363520C525W1   //define folder name

#ifdef LIBell3S1PATL363520C525W1
    #define LOGINTS 15          //learning machine log interval in seconds
    //#define FILEPATH    "C:\\SyncFolder\\Simplo\\@SMP Fuel Gauge\\Capacity Report Enhancement\\data\\LI Bell 3S1P ATL 3635 20C 5.25W-1\\LI Bell 3S1P ATL 3635 20C 5.25W-1-"

    //#define FILEPATH    "E:\\Simplo\\Data\\Cycle\\LI Bell 3S1P ATL 3635\\LI Bell 3S1P ATL 3635 45C 5.25W-3\\LI Bell 3S1P ATL 3635 45C 5.25W-3-"
    #define FILEPATH    "E:\\Simplo\\Data\\Cycle\\LI Windu Tachi Rear 3S1P ATL 2040\\LI Windu Tachi Rear 3S1P ATL 2040 40C4.8W-3\\LI Windu Tachi Rear 3S1P ATL 2040 40C4.8W-3-"

    #define FILENAME "LI Windu Tachi Rear 3S1P ATL 2040 40C4.8W-3.txt"
    #define TOTALFILENUM 262   //total cycle file numbers in in the folder
    #define V1 11400
#endif

#define MAXVOLTDIFF 1000
#define INITDELTAV 50
#define DELTAVINCRE 50
#define DELTAVEND 600

#define LINELENBUF 670

char* file_path[300];
enum {Rdate=1,Cycle,Loop,Step,Desc,LV,LI,LT,LF,CT,ohm,ManufacturerAccess,RemainingCapacityAlarm,RemainingTimeAlarm,BatteryMode,AtRate,AtRateTimeToFull,AtRateTimeToEmpty,AtRateOK,Temperature,Voltage,Current,AverageCurrent,MaxError,RelativeStateOfCharge,AbsoluteStateOfCharge,RemainingCapacity,FullChargeCapacity,RunTimeToEmpty,AverageTimeToEmpty,AverageTimeToFull,ChargingCurrent,ChargingVoltage,BatteryStatus,CycleCount,DesignCapacity,DesignVoltage,SpecificationInfo,ManufactureDate,SerialNumber,PackStatus,Packconfiguration,ManufactureData_VCELL1,ManufactureData_VCELL2,ManufactureData_VCELL3,ManufactureData_VCELL4,mAh,LmAh,WH,LWH};

int cycle_file = 0;
int current_learning_machine = 0;
int voltage_learning_machine = 0;
float temperature_pack = 0;
int voltage_pack = 0;

char* getfield(char* line, int num)
{
    char* tok;
    for (tok = strtok(line, ",");
            tok && *tok;
            tok = strtok(NULL, ",\n"))
    {
        if (!--num)
            return tok;
    }
    return NULL;
}

// C function to convert string to integer without using atoi function
int toString(char a[]) {
  int c, sign, offset, n;

  if (a[0] == '-') {  // Handle negative integers
    sign = -1;
  }

  if (sign == -1) {  // Set starting position to convert
    offset = 1;
  }
  else {
    offset = 0;
  }

  n = 0;

  for (c = offset; a[c] != '\0'; c++) {
    n = n * 10 + a[c] - '0';
  }

  if (sign == -1) {
    n = -n;
  }

  return n;
}

int capacity_chg_mAs = 0;
float capacity_chg_mAh = 0;

int alg_charging(int deltaV,int total_chg_row,int in_chg_count)
{
    //printf("temperature:%f\r\n",temperature_pack);
    //printf("voltage_pack:%d\r\n",voltage_pack);
    //printf("%d\r\n",total_chg_row);

    if(deltaV == INITDELTAV)
    {
        capacity_chg_mAs += current_learning_machine;
        capacity_chg_mAh = capacity_chg_mAs * LOGINTS/ 3600.0;
    }

    return 0;
}

FILE *pResultFile;
int v1v2_t = 0;
int voltage_abnor_diff_test = 0; //0:Normal, 1:Voltage abnormal jump or drop

int capacity_dchg_mAs = 0;
float capacity_dchg_mAh = 0;

int alg_discharging(int deltaV,int total_dchg_row,int in_dchg_count,int cycle_file)
{
    static int v1_t = 0;
    static int v2_t = 0;
    static int V1firstenter = 1;
    static int V2firstenter = 1;

    if(voltage_pack <= V1 && V1firstenter == 1)    //reached V1 voltage for the first time
    {
        v1_t = in_dchg_count * LOGINTS;     //record the time
        V1firstenter = 0;  //to prevent entering again
    }
    else if(voltage_pack <= (V1 - deltaV) && V2firstenter == 1)    //reached V2 voltage (V1 - deltaV) for the first time
    {
        v2_t = in_dchg_count * LOGINTS;     //record the time
        V2firstenter = 0;  //to prevent entering again
    }

    v1v2_t = v2_t - v1_t;

    if(in_dchg_count == total_dchg_row-1)   //dchg is done
    {
        V1firstenter = 1;   //release the condition when dchg is done
        V2firstenter = 1;   //release the condition when dchg is done
    }

    if(deltaV == INITDELTAV)
    {
        capacity_dchg_mAs -= current_learning_machine;
        capacity_dchg_mAh = capacity_dchg_mAs * LOGINTS/ 3600.0;
    }

    return 0;
}

int alg_rest()
{

    return 0;
}

//do things according to learning machine Desc state (CHG, DCHG, or REST)
int desc_state(int alg_mode, int cycle_file, int *file_integrity_check, int *total_row_num, int *total_chg_row_num, int *total_dchg_row_num, int *total_rest_row_num, int deltaV)
{
    int i = 0;
    int in_dchg_count = 0;
    int in_chg_count = 0;
    int current_chg[*total_chg_row_num];
    int current_dchg[*total_dchg_row_num];

    static int v_old = 0;
    static int v_new = 0;

    char* cdesc;

    FILE* stream1 = fopen(*file_path, "r");
    char line[LINELENBUF];

    if(NULL==stream1)
    {
        printf("Open file error:%d\r\n",cycle_file);

        return 0;
    }

    if(alg_mode)
    {
        //if the file did not pass the file integrity check, there is no point in entering alg mode
        if(*file_integrity_check == 0 /*&& voltage_abnor_diff_test == 0*/) //pass file integrity check
        {
            alg_mode = 1;
        }
        else //did not pass
        {
            alg_mode = 0;
        }

        //init total row num count to prevent accumulation due to multiple calls of desc_state()
        *total_row_num = -1;    //ignore the first row of the .csv (data description)
    }
    else
    {
        //init total row num count to prevent accumulation due to multiple calls of desc_state()
        *total_row_num = -1;    //ignore the first row of the .csv (data description)
        *total_chg_row_num = 0;
        *total_dchg_row_num = 0;
        *total_rest_row_num = 0;
    }

    while (fgets(line, LINELENBUF, stream1) && *file_integrity_check == 0)  //not end of file and passed file integrity check
    {
        char* tmp_desc          = strdup(line);
        char* tmp_LI            = strdup(line);
        char* tmp_Temperature   = strdup(line);
        char* tmp_Voltage       = strdup(line);

        cdesc = getfield(tmp_desc, Desc);   //read Desc from tmp_desc to cdesc
        free(tmp_desc);

        current_learning_machine = toString(getfield(tmp_LI, LI));  //read LI from tmp_LI and convert string to int
        free(tmp_LI);

        temperature_pack = strtof(getfield(tmp_Temperature, Temperature),NULL); //read Temperature from tmp_Temperature and convert string to float
        free(tmp_Temperature);

        voltage_pack = toString(getfield(tmp_Voltage, Voltage));    //read Voltage from tmp_Voltage and convert string to int
        free(tmp_Voltage);

        if(0==strcmp("CHG",cdesc))  //current read line is in CHG mode
        {
            if(alg_mode)
            {
                //do things while charging in real time
                alg_charging(deltaV,*total_chg_row_num,in_chg_count);
                current_chg[in_chg_count] = current_learning_machine;
                in_chg_count++;
            }
            else
            {
                (*total_chg_row_num)++; //accumulate CHG row count
            }
        }
        else if(0==strcmp("DCHG",cdesc))    //current read line is in DCHG mode
        {
            if(alg_mode)
            {
                //do things while discharging in real time
                alg_discharging(deltaV,*total_dchg_row_num,in_dchg_count,cycle_file);
                current_dchg[in_dchg_count] = current_learning_machine;
                in_dchg_count++;
            }
            else
            {
                (*total_dchg_row_num)++;    //accumulate DCHG row count

                v_new = voltage_pack;

                if(((v_new - v_old < -MAXVOLTDIFF) || (v_new - v_old > MAXVOLTDIFF)) && (*total_dchg_row_num) > 1)    //unexpected voltage jump or drop during dchg
                {
                    voltage_abnor_diff_test = 1;
                }

                v_old = v_new;

            }
        }
        else if(0==strcmp("REST",cdesc))    //current read line is in REST mode
        {
            if(alg_mode)
            {
                //do things while rest in real time
                alg_rest();
            }
            else
            {
                (*total_rest_row_num)++;    //accumulate REST row count
            }
        }

        (*total_row_num)++; //accumulate total row count. Total row should be equal to CHG + DCHG + REST row count, otherwise is will not pass file integrity check
    }

    if((!alg_mode) && (*file_integrity_check == 0) && (*total_row_num != *total_chg_row_num + *total_dchg_row_num + *total_rest_row_num))  //.csv data integrity check from Desc
    {
        *file_integrity_check = 1;  //did not pass the check
        printf("Warning! Unknown Desc in the .csv! Data may be incomplete!cycle:%d\r\n",cycle_file);
        //printf("total row \t= %d\r\ntotal chg row \t= %d\r\ntotal dchg row \t= %d\r\ntotal rest row \t= %d\r\n",*total_chg_row_num,*total_chg_row_num,*total_dchg_row_num,*total_rest_row_num);
    }

    fclose(stream1);
    return 0;
}

//merge two strings together
char* concat(const char *s1, const char *s2)
{
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char *result = malloc(len1+len2+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    memcpy(result, s1, len1);
    memcpy(result+len1, s2, len2+1);//+1 to copy the null-terminator
    return result;
}

int total_row = -1;             //total rows in the file
int total_chg_row = 0;          //total rows in learning machine charging mode
int total_dchg_row = 0;         //total rows in learning machine discharging mode
int total_rest_row = 0;         //total rows in learning machine rest mode
int file_integrity_check = 0;   //0:Normal, 1:File integrity issue

int main()
{
    char* combined_str[300];
    char* combined_str2[300];
    char* combined_str3[300];
    const char st1[] = FILEPATH;
    char st2[5];
    const char st3[] = ".csv";
    const char st4[] = "00";
    const char st5[] = "0";

    int file_integrity_check = 0; //0:Normal, 1:File integrity issue

    int deltaV = 0;     //the voltage difference between v1 and v2

    pResultFile = fopen(FILENAME,"w");  //create new blank file

    if( NULL == pResultFile )
    {
        printf( "open failure" );

        return 1;
    }

    fclose(pResultFile);

    pResultFile = fopen(FILENAME,"a");

    fprintf(pResultFile, "Cycle\t");

    for(deltaV = INITDELTAV;deltaV <= DELTAVEND; deltaV+=DELTAVINCRE) //decrease v2 for every cycle
    {
        fprintf(pResultFile, "%d mV\t", V1 - deltaV);
    }

    fprintf(pResultFile, "DCHG Capacity\tCHG Capacity");
    fprintf(pResultFile, "\n");

    for(cycle_file = 1; cycle_file <= TOTALFILENUM/*TOTALFILENUM*/; cycle_file++)   //cycle all the files in the folder
    {
        if(cycle_file < 10) //add "00": "1" -> "001"
        {
            sprintf(st2,"%d",cycle_file);   //convert int to string
            *combined_str = concat(st1,st4);
            *combined_str2 = concat(*combined_str,st2);
            *combined_str3 = concat(*combined_str2,st3);
            *file_path = *combined_str3;
        }
        else if(cycle_file < 100)   //add "0": "10" -> "010"
        {
            sprintf(st2,"%d",cycle_file);   //convert int to string
            *combined_str = concat(st1,st5);
            *combined_str2 = concat(*combined_str,st2);
            *combined_str3 = concat(*combined_str2,st3);
            *file_path = *combined_str3;
        }
        else
        {
            sprintf(st2,"%d",cycle_file);   //convert int to string
            *combined_str = concat(st1,st2);
            *combined_str2 = concat(*combined_str,st3);
            *file_path = *combined_str2;
        }

        capacity_dchg_mAs = 0;
        capacity_dchg_mAh = 0;
        capacity_chg_mAs = 0;
        capacity_chg_mAh = 0;

        file_integrity_check = 0;
        //always call desc_state(0,,,,) before desc_state(1,,,,)
        desc_state(0,cycle_file,&file_integrity_check,&total_row,&total_chg_row,&total_dchg_row,&total_rest_row,deltaV);    //calculate how many rows of CHG, DCHG and REST modes

        if(!file_integrity_check && !voltage_abnor_diff_test)
        {
            fprintf(pResultFile, "%d\t", cycle_file);
            //printf("%d\t", cycle_file);

            for(deltaV = INITDELTAV;deltaV <= DELTAVEND; deltaV+=DELTAVINCRE) //decrease v2 for every cycle
            {
                desc_state(1,cycle_file,&file_integrity_check,&total_row,&total_chg_row,&total_dchg_row,&total_rest_row,deltaV);    //execute algorithm

                fprintf(pResultFile, "%d\t", v1v2_t);
                //printf("%d\t", v1v2_t);
            }

            fprintf(pResultFile,"%f\t%f\t",capacity_dchg_mAh,capacity_chg_mAh);
            //printf("cycle:\t%d\tdchg capacity:%f\r\n",cycle_file,capacity_dchg_mAh);
            fprintf(pResultFile, "\n");
            //printf("\n");
        }
        else
        {
            if(voltage_abnor_diff_test)  //Unexpected voltage jump or drop during dchg
            {
                printf("Warning! Unexpected voltage jump or drop during dchg! cycle:%d\r\n",cycle_file);
            }

            voltage_abnor_diff_test = 0;
            fprintf(pResultFile, "%d\n", cycle_file);
        }

        printf("%f%%\t%d\r",100.0*cycle_file/TOTALFILENUM,cycle_file);
        fflush(stdout);
    }

    fclose(pResultFile);
    return 0;
}
