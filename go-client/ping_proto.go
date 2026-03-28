package main

import (
	"fmt"
	"log"

	serial "go.bug.st/serial.v1"
)

const (
	PROTO_SYNC_BYTE = 0xAA
	CMD_PING        = 0x10
	CMD_INFO        = 0x11
	CMD_SET_BLE     = 0x12
	CMD_SET_KEY     = 0x13
	CMD_IDENTIFY    = 0x14

	MSG_BTN_EVENT   = 0x01
	MSG_KNOB_ROT    = 0x02
	MSG_KNOB_CLICK  = 0x03
	MSG_HEARTBEAT   = 0x04
	MSG_DEVICE_INFO = 0x05
	MSG_ACK         = 0x06
	MSG_IDENTIFY    = 0x07
)

func crc8Maxim(data []byte) byte {
	var crc byte = 0x00
	for _, b := range data {
		crc ^= b
		for i := 0; i < 8; i++ {
			if crc&0x80 != 0 {
				crc = (crc << 1) ^ 0x31
			} else {
				crc <<= 1
			}
		}
	}
	return crc
}

func buildFrame(cmd byte, payload []byte) []byte {
	payloadLen := len(payload)
	totalLen := 1 + payloadLen + 1 // TYPE + PAYLOAD + CRC
	frame := make([]byte, 0, 3+totalLen)
	frame = append(frame, PROTO_SYNC_BYTE)
	frame = append(frame, byte(totalLen))
	frame = append(frame, cmd)
	if payloadLen > 0 {
		frame = append(frame, payload...)
	}

	crcPayload := make([]byte, 1+payloadLen)
	crcPayload[0] = cmd
	if payloadLen > 0 {
		copy(crcPayload[1:], payload)
	}

	crc := crc8Maxim(crcPayload)
	frame = append(frame, crc)
	return frame
}

func decodeBtnEvent(payload []byte) string {
	if len(payload) < 2 {
		return "BTN_EVENT: payload invalido"
	}
	idx := payload[0]
	action := payload[1]
	state := "released"
	if action == 0x01 {
		state = "pressed"
	}
	return fmt.Sprintf("BTN_EVENT idx=%d %s", idx, state)
}

func decodeKnobRotate(payload []byte) string {
	if len(payload) < 1 {
		return "KNOB_ROTATE: invalido"
	}
	if payload[0] == 0x01 {
		return "KNOB_ROTATE: CW"
	}
	return "KNOB_ROTATE: CCW"
}

func decodeKnobClick(payload []byte) string {
	if len(payload) < 1 {
		return "KNOB_CLICK: invalido"
	}
	return fmt.Sprintf("KNOB_CLICK count=%d", payload[0])
}

func decodeHeartbeat(payload []byte) string {
	if len(payload) < 5 {
		return "HEARTBEAT: invalido"
	}
	uptime := uint32(payload[0]) | uint32(payload[1])<<8 | uint32(payload[2])<<16 | uint32(payload[3])<<24
	ble := payload[4]
	return fmt.Sprintf("HEARTBEAT uptime=%ds ble=%t", uptime, ble == 0x01)
}

func decodeDeviceInfo(payload []byte) string {
	if len(payload) < 6 {
		return "DEVICE_INFO: invalido"
	}
	return fmt.Sprintf("DEVICE_INFO v%d.%d.%d buttons=%d knobs=%d flags=0x%02X",
		payload[0], payload[1], payload[2], payload[3], payload[4], payload[5])
}

func decodeAck(payload []byte) string {
	if len(payload) < 2 {
		return "ACK: invalido"
	}
	status := "OK"
	if payload[1] == 0x01 {
		status = "UNKNOWN_CMD"
	}
	if payload[1] == 0x02 {
		status = "BAD_PAYLOAD"
	}
	return fmt.Sprintf("ACK cmd=0x%02X status=%s", payload[0], status)
}

func decodeIdentify(payload []byte) string {
	if len(payload) < 12 {
		return "IDENTIFY: invalido"
	}
	magic := fmt.Sprintf("%02X %02X %02X %02X", payload[0], payload[1], payload[2], payload[3])
	id := string(payload[4:12])
	return fmt.Sprintf("IDENTIFY magic=[%s] hwid=%q", magic, id)
}

func main() {
	mode := &serial.Mode{BaudRate: 115200, Parity: serial.NoParity, StopBits: serial.OneStopBit}
	portName := "COM4" // Ajuste
	port, err := serial.Open(portName, mode)
	if err != nil {
		log.Fatalf("Falha ao abrir porta %s: %v", portName, err)
	}
	defer port.Close()

	ping := buildFrame(CMD_PING, nil)
	fmt.Printf("Enviando CMD_PING: % X\n", ping)
	_, err = port.Write(ping)
	if err != nil {
		log.Fatalf("Falha ao enviar ping: %v", err)
	}

	// Leitura/parse em loop
	rxState := 0 // 0=SYNC 1=LEN 2=DATA
	var rxLen byte
	var rxBuf []byte
	var rxPos int

	var lastBtnStates [10]byte
	var lastKnobPhrase string

	for {
		buf := make([]byte, 128)
		n, err := port.Read(buf)
		if err != nil {
			log.Printf("Erro read: %v", err)
			continue
		}
		if n == 0 {
			continue
		}

		for i := 0; i < n; i++ {
			b := buf[i]
			switch rxState {
			case 0: // SYNC
				if b == PROTO_SYNC_BYTE {
					rxState = 1
				}

			case 1: // LEN
				if b < 2 || b > 34 {
					rxState = 0
				} else {
					rxLen = b
					rxBuf = make([]byte, rxLen)
					rxPos = 0
					rxState = 2
				}

			case 2: // DATA
				rxBuf[rxPos] = b
				rxPos++
				if rxPos >= int(rxLen) {
					// valida CRC
					payload := rxBuf[:rxLen-1]
					crc := rxBuf[rxLen-1]
					if crc8Maxim(payload) != crc {
						fmt.Println("CRC inválido, descartando frame")
						rxState = 0
						break
					}

					msgType := payload[0]
					pl := payload[1:]
					var text string
					switch msgType {
					case MSG_BTN_EVENT:
						text = decodeBtnEvent(pl)
						if len(pl) >= 2 {
							idx := pl[0]
							state := pl[1]
							if idx < 10 && lastBtnStates[idx] != state {
								lastBtnStates[idx] = state
								fmt.Printf(" [CHANGED] %s\n", text)
							} else {
								fmt.Printf(" %s\n", text)
							}
						} else {
							fmt.Printf(" %s\n", text)
						}
					case MSG_KNOB_ROT:
						text = decodeKnobRotate(pl)
						if text != lastKnobPhrase {
							lastKnobPhrase = text
							fmt.Printf(" [CHANGED] %s\n", text)
						} else {
							fmt.Printf(" %s\n", text)
						}
					case MSG_KNOB_CLICK:
						text = decodeKnobClick(pl)
						fmt.Printf(" %s\n", text)
					case MSG_HEARTBEAT:
						text = decodeHeartbeat(pl)
						fmt.Printf(" %s\n", text)
					case MSG_DEVICE_INFO:
						text = decodeDeviceInfo(pl)
						fmt.Printf(" %s\n", text)
					case MSG_ACK:
						text = decodeAck(pl)
						fmt.Printf(" %s\n", text)
					case MSG_IDENTIFY:
						text = decodeIdentify(pl)
						fmt.Printf(" %s\n", text)
					default:
						fmt.Printf("MSG UNKNOWN 0x%02X payload=% X\n", msgType, pl)
					}

					rxState = 0
				}
			}
		}
	}
}
