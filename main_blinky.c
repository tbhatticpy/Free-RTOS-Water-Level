/*Project by:
		Muhammad Talha Bhatti
		Sikandar Bakht Kiani
		Zain Ishtiaq*/


/* Standard includes. */
#include <stdio.h>
#include <conio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Priorities at which the tasks are created. */
#define mainQUEUE_LevelSensor_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define	mainQUEUE_Motor_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
uint32_t mainWater_Level = 0;
/* The rate at which data is sent to the queues.  The times are converted from
milliseconds to ticks using the pdMS_TO_TICKS() macro. */
#define mainTASK_SEND_FREQUENCY_MS			pdMS_TO_TICKS( 200UL )
#define mainTIMER_SEND_FREQUENCY_MS			pdMS_TO_TICKS( 2000UL )

/* The number of items the queue can hold at once. */
#define mainQUEUE_LENGTH					( 1 )

/* The values sent to the queue receive task from the queue send task and the
queue send software timer respectively. */

//#define mainMotor_start       (1)
//#define mainMotor_stop        (0)
#define mainLow_Water_Level_Threshold			( 10 )
#define mainMedium_Water_Level_Threshold			( 20 )
#define mainHigh_Water_Level_Threshold			( 30 )

typedef enum
{
	mainMotor_start,
	mainMotor_stop
} motor_status;
typedef enum
{
	mainMotor_forward,
	mainMotor_reverse
} motor_direction;
typedef struct
{
	motor_status motor_status;
	motor_direction motor_direction;
} motor;

xSemaphoreHandle gatekeeper = 0;
/*-----------------------------------------------------------*/

/*
 * The tasks as described in the comments at the top of this file.
 */
 //static void prvQueueReceiveTask( void *pvParameters );
static void prvMotortoFillTask(void* pvParameters);
static void prvMotortoEmptyTask(void* pvParameters);
static void prvLevelSensorTask(void* pvParameters);

/*
 * The callback function executed when the software timer expires.
 */
static void prvQueueSendTimerCallback(TimerHandle_t xTimerHandle);

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xLevelQueue = NULL;
static QueueHandle_t xMotorQueue = NULL;
/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

/*-----------------------------------------------------------*/

/*** SEE THE COMMENTS AT THE TOP OF THIS FILE ***/
void main_blinky(void)
{
	const TickType_t xTimerPeriod = mainTIMER_SEND_FREQUENCY_MS;
	gatekeeper = xSemaphoreCreateMutex();

	/* Create the queue. */
	xLevelQueue = xQueueCreate(mainQUEUE_LENGTH, sizeof(uint32_t));
	xMotorQueue = xQueueCreate(mainQUEUE_LENGTH, sizeof(motor));

	if ((xLevelQueue != NULL) & (xMotorQueue != NULL))
	{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate(prvLevelSensorTask,			/* The function that implements the task. */
			"Motor Feedback", 							/* The text name assigned to the task - for debug only as it is not used by the kernel. */
			configMINIMAL_STACK_SIZE, 		/* The size of the stack to allocate to the task. */
			NULL, 							/* The parameter passed to the task - not used in this simple case. */
			mainQUEUE_LevelSensor_TASK_PRIORITY,/* The priority assigned to the task. */
			NULL);							/* The task handle is not required, so NULL is passed. */

		xTaskCreate(prvMotortoFillTask, "Fill Tank", configMINIMAL_STACK_SIZE, NULL, mainQUEUE_Motor_TASK_PRIORITY, NULL);
		xTaskCreate(prvMotortoEmptyTask, "Empty Tank", configMINIMAL_STACK_SIZE, NULL, mainQUEUE_Motor_TASK_PRIORITY, NULL);

		/* Create the software timer, but don't start it yet. */
		xTimer = xTimerCreate("Timer",				/* The text name assigned to the software timer - for debug only as it is not used by the kernel. */
			xTimerPeriod,		/* The period of the software timer in ticks. */
			pdFALSE,			/* xAutoReload is set to pdFALSE, so this is a one-shot timer. */
			NULL,				/* The timer's ID is not used. */
			prvQueueSendTimerCallback);/* The function executed when the timer expires. */

		xTimerStart(xTimer, 0); /* The scheduler has not started so use a block time of 0. */

		/* Start the tasks and timer running. */
		vTaskStartScheduler();
	}

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	for (;; );
}
/*-----------------------------------------------------------*/


static void prvLevelSensorTask(void* pvParameters)
{
	uint32_t ulReceivedValue;
	motor mlValueToSend;
	/* Prevent the compiler warning about the unused parameter. */
	(void)pvParameters;


	for (;; )
	{
		/* Wait until something arrives in the queue - this task will block
		indefinitely provided INCLUDE_vTaskSuspend is set to 1 in
		FreeRTOSConfig.h.  It will not use any CPU time while it is in the
		Blocked state. */

		xQueueReceive(xLevelQueue, &ulReceivedValue, portMAX_DELAY);

		/*  To get here something must have been received from the queue, but
		is it an expected value?  Normally calling printf() from a task is not
		a good idea.  Here there is lots of stack space and only one task is
		using console IO so it is ok.  However, note the comments at the top of
		this file about the risks of making Windows system calls (such as
		console output) from a FreeRTOS task. */
		if (ulReceivedValue > 0 && ulReceivedValue < mainLow_Water_Level_Threshold)
		{
			printf("Water Levels is low %d\r\n", ulReceivedValue);
		}
		else if (ulReceivedValue >= mainLow_Water_Level_Threshold && ulReceivedValue < mainMedium_Water_Level_Threshold)
		{
			printf("Water Level is Medium %d\r\n", ulReceivedValue);
		}
		else if (ulReceivedValue >= mainMedium_Water_Level_Threshold && ulReceivedValue < mainHigh_Water_Level_Threshold)
		{
			printf("Water Level is High %d\r\n", ulReceivedValue);
		}

		else
		{
			mlValueToSend.motor_status = mainMotor_stop;
			if (ulReceivedValue >= mainHigh_Water_Level_Threshold) {
				mlValueToSend.motor_direction = mainMotor_reverse;
				printf("Maximum Level reached. %d Stopping the motor \r\n", ulReceivedValue);
			}
			else {
				mlValueToSend.motor_direction = mainMotor_forward;
				printf("No water remained. %d Stopping the motor \r\n", ulReceivedValue);
			}
			if (_kbhit() != 0)
			{
				/* Remove the key from the input buffer.*/
				(void)_getch();
				mlValueToSend.motor_status = mainMotor_start;
			}
			xQueueSend(xMotorQueue, &mlValueToSend, 0U);
		}

	}
}
static void prvMotortoFillTask(void* pvParameters)
{
	TickType_t xNextWakeTime;
	const TickType_t xBlockTime = mainTASK_SEND_FREQUENCY_MS;
	motor Msignal;
	//uint32_t ulValueToSend;// = mainVALUE_SENT_FROM_TASK;
		/* Prevent the compiler warning about the unused parameter. */
	(void)pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();
	printf("Waiting for Motor to Start\r\n");
	xQueueReceive(xMotorQueue, &Msignal, portMAX_DELAY);  //get the start signal

	for (;; )
	{
		/* Place this task in the blocked state until it is time to run again.
		The block time is specified in ticks, pdMS_TO_TICKS() was used to
		convert a time specified in milliseconds into a time specified in ticks.
		While in the Blocked state this task will not consume any CPU time. */
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);
		/* Send to the queue - causing the queue receive task to unblock and
		write to the console.  0 is used as the block time so the send operation
		will not block - it shouldn't need to block as the queue should always
		have at least one space at this point in the code. */
		if (xSemaphoreTake(gatekeeper, 100U) == pdPASS) {
			xQueueReceive(xMotorQueue, &Msignal, 0U);

			if (Msignal.motor_status == mainMotor_start && Msignal.motor_direction == mainMotor_forward)
				++mainWater_Level;
			xQueueSend(xLevelQueue, &mainWater_Level, 0U);
			xSemaphoreGive(gatekeeper);
		}
		else
			printf("failed1");
	}
}


static void prvMotortoEmptyTask(void* pvParameters)
{
	TickType_t xNextWakeTime;
	const TickType_t xBlockTime = mainTASK_SEND_FREQUENCY_MS;
	motor Msignal;
	//uint32_t ulValueToSend;// = mainVALUE_SENT_FROM_TASK;
	/* Prevent the compiler warning about the unused parameter. */
	(void)pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();
	xQueueReceive(xMotorQueue, &Msignal, portMAX_DELAY);  //get the start signal

	for (;; )
	{
		/* Place this task in the blocked state until it is time to run again.
		The block time is specified in ticks, pdMS_TO_TICKS() was used to
		convert a time specified in milliseconds into a time specified in ticks.
		While in the Blocked state this task will not consume any CPU time. */
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);
		/* Send to the queue - causing the queue receive task to unblock and
		write to the console.  0 is used as the block time so the send operation
		will not block - it shouldn't need to block as the queue should always
		have at least one space at this point in the code. */
		//for (int i = 0; i < 11; i++)
		xQueueReceive(xMotorQueue, &Msignal, 100U);
		if (xSemaphoreTake(gatekeeper, 100U) == pdPASS) {
			if (Msignal.motor_status == mainMotor_start && Msignal.motor_direction == mainMotor_reverse)
				--mainWater_Level;
			xQueueSend(xLevelQueue, &mainWater_Level, 0U);

			//if (mainWater_Level <= 0)
			xSemaphoreGive(gatekeeper);
		}
		else
			printf("failed");
	}
}

/*-----------------------------------------------------------*/

static void prvQueueSendTimerCallback(TimerHandle_t xTimerHandle)
{
	const motor ulValueToSend = { mainMotor_start, mainMotor_forward };

	/* This is the software timer callback function.  The software timer has a
	period of two seconds and is reset each time a key is pressed.  This
	callback function will execute if the timer expires, which will only happen
	if a key is not pressed for two seconds. */

	/* Avoid compiler warnings resulting from the unused parameter. */
	(void)xTimerHandle;

	/* Send to the queue - causing the queue receive task to unblock and
	write out a message.  This function is called from the timer/daemon task, so
	must not block.  Hence the block time is set to 0. */
	xQueueSend(xMotorQueue, &ulValueToSend, 0U);
}
/*-----------------------------------------------------------*/
