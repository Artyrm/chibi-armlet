/*
 * cmd_uart.cpp
 *
 *  Created on: 15.04.2013
 *      Author: kreyl
 */

#include "cmd_uart.h"
#include <string.h>
#include "application.h"

CmdUart_t Uart;

static inline void FPutChar(char c) { Uart.IPutChar(c); }

void CmdUart_t::IPutChar(char c) {
    *PWrite++ = c;
    if(PWrite >= &TXBuf[UART_TXBUF_SIZE]) PWrite = TXBuf;   // Circulate buffer
}

void CmdUart_t::Printf(const char *format, ...) {
    uint32_t MaxLength = (PWrite < PRead)? (PRead - PWrite) : ((UART_TXBUF_SIZE + PRead) - PWrite);
    va_list args;
    va_start(args, format);
    IFullSlotsCount += kl_vsprintf(FPutChar, MaxLength, format, args);
    va_end(args);

    // Start transmission if Idle
    if(IDmaIsIdle) {
        IDmaIsIdle = false;
        dmaStreamSetMemory0(UART_DMA_TX, PRead);
        uint32_t PartSz = (TXBuf + UART_TXBUF_SIZE) - PRead;    // Char count from PRead to buffer end
        ITransSize = (IFullSlotsCount > PartSz)? PartSz : IFullSlotsCount;  // How many to transmit now
        dmaStreamSetTransactionSize(UART_DMA_TX, ITransSize);
        dmaStreamSetMode(UART_DMA_TX, UART_DMA_TX_MODE);
        dmaStreamEnable(UART_DMA_TX);
    }
}

#if UART_RX_ENABLED
static inline uint8_t TryConvertToDigit(uint8_t b, uint8_t *p) {
    if((b >= '0') and (b <= '9')) {
        *p = b - '0';
        return OK;
    }
    else if((b >= 'A') and (b <= 'F')) {
        *p = 0x0A + b - 'A';
        return OK;
    }
    else return FAILURE;
}
static inline bool IsStart(char c) { return (c == '#'); }
static inline bool IsEnd(char c) { return (c == '\r') or (c == '\n'); }

void CmdUart_t::PollRx() {
    int32_t Sz = UART_RXBUF_SZ - UART_DMA_RX->channel->CNDTR;   // Number of bytes copied to buffer since restart
    if(Sz != SzOld) {
        int32_t ByteCnt = Sz - SzOld;
        if(ByteCnt < 0) ByteCnt += UART_RXBUF_SZ;   // Handle buffer circulation
        SzOld = Sz;
        for(int32_t i=0; i<ByteCnt; i++) {          // Iterate received bytes
            char c = IRxBuf[RIndx++];
            if(IsStart(c)) PCmdWrite->StartRx();
            else if(IsEnd(c)) CompleteCmd();
            else if(PCmdWrite->RxState == rsReceiving) PCmdWrite->PutChar(c);
            if(RIndx >= UART_RXBUF_SZ) RIndx = 0;
        }
    }
}

void CmdUart_t::CompleteCmd() {
    if(PCmdWrite->IsEmpty()) return;
    chSysLock();
    PCmdWrite->Finalize();
    PCmdRead = PCmdWrite;
    PCmdWrite = (PCmdWrite == &ICmd[0])? &ICmd[1] : &ICmd[0];
    PCmdWrite->Reset();
    chSysUnlock();
    App.OnUartCmd(PCmdRead);
}

// ==== Cmd ====
void Cmd_t::PutChar(char c) {
    if(WriteIndx >= UART_CMD_BUF_SZ) Reset();
    // Check if delimiter
    else if(c == ',') {
        IBuf[WriteIndx++] = 0;
        if(ChunkCnt < CHUNK_CNT) Chunks[ChunkCnt++] = &IBuf[WriteIndx];
        WasComma = true;
        WasSpace = false;
    }
    else if(c == ' ') {
        if(!(WasComma or WasSpace)) {
            IBuf[WriteIndx++] = 0;
            if(ChunkCnt < CHUNK_CNT) Chunks[ChunkCnt++] = &IBuf[WriteIndx];
        }
        WasComma = false;
        WasSpace = true;
    }
    else {
        IBuf[WriteIndx++] = c;
        WasSpace = false;
        WasComma = false;
    }
}
void Cmd_t::Finalize() {
    if(WriteIndx == UART_CMD_BUF_SZ) IBuf[UART_CMD_BUF_SZ-1] = 0;
    else IBuf[WriteIndx++] = 0;
}


/*
 *     uint8_t digit=0;
    if(b == '#') RxState = rsCmdCode1; // If # is received anywhere, start again
    else switch(RxState) {
        case rsCmdCode1:
            if(TryConvertToDigit(b, &digit) == OK) {
                CmdCode = digit << 4;
                RxState = rsCmdCode2;
            }
            else IResetCmd();
            break;

        case rsCmdCode2:
            if(TryConvertToDigit(b, &digit) == OK) {
                CmdCode |= digit;
                RxState = rsData1;
            }
            else IResetCmd();
            break;

        case rsData1:
            if(TryConvertToDigit(b, &digit) == OK) {
                *PCmdWrite = digit << 4;
                RxState = rsData2;
            }
            else if(IsDelimiter(b)) return; // skip delimiters
            else if(IsEnd(b)) {
                App.OnUartCmd(CmdCode, CmdData, (PCmdWrite - CmdData));
                IResetCmd();
            }
            else IResetCmd();
            break;

        case rsData2:
            if(TryConvertToDigit(b, &digit) == OK) {
                *PCmdWrite |= digit;
                RxState = rsData1;  // Prepare to rx next byte
                if(PCmdWrite < (CmdData + (UART_CMDDATA_SZ-1))) PCmdWrite++;
            }
            else IResetCmd(); // Delimiters and End symbols are not allowed in the middle of byte
            break;

        default: break;
    } // switch
 */

//void CmdUart_t::IProcessByte(uint8_t b) {
//
//}
#endif

// ==== Init & DMA ====
// Wrapper for TX IRQ
extern "C" {
void CmdUartTxIrq(void *p, uint32_t flags) { Uart.IRQDmaTxHandler(); }
}

void CmdUart_t::Init(uint32_t ABaudrate) {
    PWrite = TXBuf;
    PRead = TXBuf;
    IDmaIsIdle = true;
    IFullSlotsCount = 0;
    PinSetupAlterFunc(UART_GPIO, UART_TX_PIN, omPushPull, pudNone, UART_AF);

    // ==== USART configuration ====
    UART_RCC_ENABLE();
    UART->CR1 = USART_CR1_UE;     // Enable USART
    if(UART == USART1) UART->BRR = Clk.APB2FreqHz / ABaudrate;
    else               UART->BRR = Clk.APB1FreqHz / ABaudrate;
    UART->CR2 = 0;
    // ==== DMA ====
    dmaStreamAllocate     (UART_DMA_TX, IRQ_PRIO_HIGH, CmdUartTxIrq, NULL);
    dmaStreamSetPeripheral(UART_DMA_TX, &UART->DR);
    dmaStreamSetMode      (UART_DMA_TX, UART_DMA_TX_MODE);

#if UART_RX_ENABLED
    UART->CR1 = USART_CR1_TE | USART_CR1_RE;        // TX & RX enable
    UART->CR3 = USART_CR3_DMAT | USART_CR3_DMAR;    // Enable DMA at TX & RX

    PinSetupAlterFunc(UART_GPIO, UART_RX_PIN,  omOpenDrain, pudPullUp, UART_AF);

    dmaStreamAllocate     (UART_DMA_RX, IRQ_PRIO_LOW, nullptr, NULL);
    dmaStreamSetPeripheral(UART_DMA_RX, &UART->DR);
    dmaStreamSetMemory0   (UART_DMA_RX, IRxBuf);
    dmaStreamSetTransactionSize(UART_DMA_RX, UART_RXBUF_SZ);
    dmaStreamSetMode      (UART_DMA_RX, UART_DMA_RX_MODE);
    dmaStreamEnable       (UART_DMA_RX);
#else
    UART->CR1 = USART_CR1_TE;     // Transmitter enabled
    UART->CR3 = USART_CR3_DMAT;   // Enable DMA at transmitter
#endif
    UART->CR1 |= USART_CR1_UE;    // Enable USART
}

// ==== TX DMA IRQ ====
void CmdUart_t::IRQDmaTxHandler() {
    dmaStreamDisable(UART_DMA_TX);    // Registers may be changed only when stream is disabled
    IFullSlotsCount -= ITransSize;
    PRead += ITransSize;
    if(PRead >= (TXBuf + UART_TXBUF_SIZE)) PRead = TXBuf; // Circulate pointer

    if(IFullSlotsCount == 0) IDmaIsIdle = true; // Nothing left to send
    else {  // There is something to transmit more
        dmaStreamSetMemory0(UART_DMA_TX, PRead);
        uint32_t PartSz = (TXBuf + UART_TXBUF_SIZE) - PRead;
        ITransSize = (IFullSlotsCount > PartSz)? PartSz : IFullSlotsCount;
        dmaStreamSetTransactionSize(UART_DMA_TX, ITransSize);
        dmaStreamSetMode(UART_DMA_TX, UART_DMA_TX_MODE);
        dmaStreamEnable(UART_DMA_TX);    // Restart DMA
    }
}
