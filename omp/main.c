


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <omp.h>
int getRandRangeInt(const int min, const int max) {
    return rand() % (max - min + 1) + min;
}

int main(int argc, char **argv) 
{

	srand(1);
	FILE *fp;
	if ((fp = fopen("output6.txt", "w")) == NULL)return 0;
	int MAXLEN = 30000; // максимальный размер матрицы
    int curMAXLEN = 17000;
	//int MAXLEN = 1024;
	//printf("Enter size of matrix\n");
	int column=MAXLEN,line=MAXLEN;
	//scanf("%d%d", &line, &column);
	int *matrix, *vector,*answer;
	matrix = (int*)malloc(column*line*sizeof(int));
	vector = (int*)malloc(column*sizeof(int));
	answer = (int*)malloc(line*sizeof(int));
	for (int i = 0; i < line; i++)
	{
		for (int j = 0; j < column; j++)
		{
			*(matrix + i*column + j) = rand();
			*(vector+j) = rand();	
		}
	}
	int i,j;
	struct timeval tvalBefore, tvalAfter;
	struct timeval tvalFBefore, tvalFAfter;
	column = 1000; // начальный размер
	line = 1000; // начальный размер 
	for (; (column <= curMAXLEN && line <= curMAXLEN) ;){
		fprintf(fp,"%d x %d\n",column,line);
		for (int k = 0; k <=160; k+=2) // перебор ниток 
		{
			if(k == 0)k = 1;
			gettimeofday (&tvalBefore, NULL);
			for(int iteration = 0; iteration < 20; iteration++){// количество итераций
				gettimeofday (&tvalFBefore, NULL);
				#pragma omp parallel for default(none) private(i, j) shared(answer,matrix, vector, column, line)num_threads(k*2)
				for (i = 0; i < line; i++)
				{
					*(answer+i) = 0;
					for (j = 0; j < column; j++)
					{   
						*(answer+i) += (*(matrix + i*column + j))*(*(vector+j));
					}
				}
				gettimeofday (&tvalFAfter, NULL);
				fprintf(fp,"%ld ",((tvalFAfter.tv_sec - tvalFBefore.tv_sec)*1000000L
					+tvalFAfter.tv_usec) - tvalFBefore.tv_usec);
				
			}
			fprintf(fp, "%d\n",k);
			gettimeofday (&tvalAfter, NULL);
			printf("Time in microseconds: %ld microseconds %d treads size column %d lines %d\n",
					((tvalAfter.tv_sec - tvalBefore.tv_sec)*1000000L
					+tvalAfter.tv_usec) - tvalBefore.tv_usec,
					k, column, line
					);
            if(k==1)k=0;
		}
		column += 2000; // шаг в размерности массива
		line += 2000;
	}
	fclose(fp);
	return 0;
}

