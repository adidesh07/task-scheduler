#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "led.h"

void task1_handler(void);
void task2_handler(void);
void task3_handler(void);
void task4_handler(void);

void init_systik_timer(uint32_t tick_hz);
__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack);

void init_tasks_stack(void);
void enable_processor_faults(void);
__attribute__((naked)) void switch_sp_to_psp(void);
uint32_t get_psp_value(void);

uint32_t psp_of_tasks[MAX_TASKS] = {T1_STACK_START, T2_STACK_START, T3_STACK_START, T4_STACK_START};
uint32_t task_handlers[MAX_TASKS];
uint32_t current_task = 0; // task1 is running

typedef struct{
	uint32_t psp_value;
	uint32_t block_count;
	uint8_t current_state;
	void (*task_handler)(void);
}TCB_t;

TCB_t user_tasks[MAX_TASKS];

int main(void)
{
	enable_processor_faults();

	init_scheduler_stack(SCHED_STACK_START);

	task_handlers[0] = (uint32_t) task1_handler;
	task_handlers[1] = (uint32_t) task2_handler;
	task_handlers[2] = (uint32_t) task3_handler;
	task_handlers[3] = (uint32_t) task4_handler;

	init_tasks_stack();
	led_init_all();
	init_systik_timer(TICK_HZ);

	switch_sp_to_psp();

	task1_handler();

	for(;;);
}

void task1_handler(void){
	while(1){
		led_on(LED_GREEN);
		delay(DELAY_COUNT_1S);
		led_off(LED_GREEN);
		delay(DELAY_COUNT_1S);
	}
}

void task2_handler(void){
	while(1){
		led_on(LED_ORANGE);
		delay(DELAY_COUNT_500MS);
		led_off(LED_ORANGE);
		delay(DELAY_COUNT_500MS);
	}
}

void task3_handler(void){
	while(1){
		led_on(LED_BLUE);
		delay(DELAY_COUNT_250MS);
		led_off(LED_BLUE);
		delay(DELAY_COUNT_250MS);
	}
}

void task4_handler(void){
	while(1){
		led_on(LED_RED);
		delay(DELAY_COUNT_125MS);
		led_off(LED_RED);
		delay(DELAY_COUNT_125MS);
	}
}

void init_systik_timer(uint32_t tick_hz){
	uint32_t *pSRVR = (uint32_t*) 0xE000E014;
	uint32_t *pSCSR = (uint32_t*) 0xE000E010;
	uint32_t count_value = (SYSTICK_TIM_CLK / tick_hz) - 1;

	//Load val in SysTick SVR
	*pSRVR &= ~(0x00FFFFFFFF);
	*pSRVR |= count_value;

	//Do some settings in Control and status reg
	*pSCSR |= (1 << 1); // Enable SysTick exception request
	*pSCSR |= (1 << 2); // Indicate clock source to internal clock
	*pSCSR |= (1 << 0); // Enable SysTick counter
}

__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack){
	__asm volatile ("MSR MSP,%0" : : "r" (sched_top_of_stack) : );
	__asm volatile ("BX LR");
}

void init_tasks_stack(void){
	uint32_t *pPSP;

	for (int i = 0; i < MAX_TASKS; i++){
		pPSP = (uint32_t*) psp_of_tasks[i];

		pPSP--;
		*pPSP = DUMMY_XPSR; //0x01000000

		pPSP--; // PC
		*pPSP = task_handlers[i];

		pPSP--; // LR
		*pPSP = 0xFFFFFFFD;

		for (int j = 0; j < 13; j++){
			pPSP--;
			*pPSP = 0;
		}

		psp_of_tasks[i] = (uint32_t) pPSP;
	}
}

void enable_processor_faults(void){
	uint32_t *pSHCSR = (uint32_t*)0xE000ED24;

	*pSHCSR |= ( 1 << 16); //mem manage
	*pSHCSR |= ( 1 << 17); //bus fault
	*pSHCSR |= ( 1 << 18); //usage fault

}

uint32_t get_psp_value(void){
	return psp_of_tasks[current_task];
}

void save_psp_value(uint32_t current_psp_val){
	psp_of_tasks[current_task] = current_psp_val;
}

void update_next_task(void){
	current_task++;
	current_task %= MAX_TASKS;
}

__attribute__((naked)) void switch_sp_to_psp(void){
	// Init PSP with TASK1 stack start address
	// get val of psp of current task
	__asm volatile ("PUSH {LR}"); // preserve LR for main()
	__asm volatile ("BL get_psp_value");
	__asm volatile ("MSR PSP,R0"); // init psp
	__asm volatile ("POP {LR}");

	// Change SP to PSP
	__asm volatile ("MOV R0,#0x02");
	__asm volatile ("MSR CONTROL,R0");
	__asm volatile ("BX LR");

}

__attribute__((naked)) void SysTick_Handler(void){
	// Save context of current task
	// 1. Get current running task's PSP vals
	__asm volatile ("MRS R0,PSP");
	// 2. Using PSP val, store SF2 (R4 to R11)
	__asm volatile ("STMDB R0!,{R4-R11}");

	__asm volatile ("PUSH {LR}");

	// 3. Save current val of PSP
	__asm volatile ("BL save_psp_value");

	// Retrieve context of next task
	// 1. Decide next task to run
	__asm volatile ("BL update_next_task");
	// 2. Get its past PSP value
	__asm volatile ("BL get_psp_value");
	// 3. Using PSP val, retrieve SF2 (R4 to R11)
	__asm volatile ("LDM R0!,{R4-R11}");
	// 4. Update PSP and exit
	__asm volatile ("MSR PSP,R0");

	__asm volatile ("POP {LR}");
	__asm volatile ("BX LR");
}

void HardFault_Handler(void)
{
	printf("Exception: Hardfault\n");
	while(1);
}


void MemManage_Handler(void)
{
	printf("Exception : MemManage\n");
	while(1);
}

void BusFault_Handler(void)
{
	printf("Exception : BusFault\n");
	while(1);
}







