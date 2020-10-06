#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#define P 17
#define Q 14
#define F 1 << Q

/*Convert integer to fixed-point number*/
#define I2FP(n) ((n) * (F))

/*Convert fixed-point number to integer(rounding toward zero)*/
#define FP2IZ(x) ((x) / (F))

/*fixed-point number to integer(rounding toward nearest)*/
#define FP2IN(x) (x >= 0) ? (((x) + ((F) / (2))) / (F)) : (((x) - ((F) / (2))) / (F))

/*Addition(fp + fp)*/
#define ADDFF(x,y) ((x) + (y))

/*Subtraction(fp - fp)*/
#define SUBFF(x,y) ((x) - (y))

/*Subtraction(fp + int)*/
#define ADDFI(x,n) ((x) + ((n) * (F)))

/*Subtraction(fp - int)*/
#define SUBFI(x,n) ((x) - ((n) * (F)))

/*MUltiplication(fp * fp)*/
#define MULFF(x,y) ((((int64_t) x) * (y)) / (F))

/*MUltiplication(fp * int)*/
#define MULFI(x,n) ((x) * (n))

/*Division(fp / fp)*/
#define DIVFF(x,y) ((((int64_t) x) * (F)) / (y))

/*Division(fp / int)*/
#define DIVFI(x,n) ((x) / (n))

#endif