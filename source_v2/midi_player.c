#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

// MIDI serial port
#define SERIAL_PORT "/dev/ttyACM0"

// Special message for Ctrl+C
#define CTRL_C  0xFF

#define MIDI_MSG_SIZE   3

volatile sig_atomic_t should_exit = 0;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    should_exit = 1;
}

// Function to configure the serial port
int configure_serial_port(const char* port_name) {
    int serial_fd = open(port_name, O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd == -1) {
        perror("Unable to open serial port");
        exit(1);
    }

    struct termios options;
    memset(&options, 0, sizeof options);

    if (tcgetattr(serial_fd, &options) < 0) {
        perror("Failed to get terminal attributes");
        close(serial_fd);
        exit(1);
    }

    cfsetispeed(&options, B38400);  // Set baud rate to 31250 for MIDI (38400 for systems without MIDI baud rate)
    cfsetospeed(&options, B38400);

    options.c_cflag |= (CLOCAL | CREAD);    // Enable receiver and set local mode
    options.c_cflag &= ~(PARENB | PARODD);  // No parity
    options.c_cflag &= ~CSTOPB; // 1 stop bit
    options.c_cflag &= ~CSIZE;  // Clear size mask
    options.c_cflag |= CS8; // 8 data bits

    options.c_cflag &= ~CRTSCTS;    // No hardware flow control
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // No software flow control
    options.c_oflag &= ~OPOST;  // Raw output

    options.c_cc[VEOL] = 0;
    options.c_cc[VEOL2] = 0;
    options.c_cc[VEOF] = 0x04;

    // cfmakeraw(&options);    // Set raw mode
    tcflush(serial_fd, TCIOFLUSH);   // Flush the input buffer

    if (tcsetattr(serial_fd, TCSANOW, &options) < 0) {
        perror("Failed to set terminal attributes");
        close(serial_fd);
        exit(1);
    }

    return serial_fd;
}

// Function to send a MIDI message via ALSA
void send_midi_message(snd_rawmidi_t* midi_out, uint8_t* message) {
    snd_rawmidi_write(midi_out, message, 3);
    snd_rawmidi_drain(midi_out);
}

// Function to start fluidsynth
void start_synth_and_connect() {
    // Start fluidsynth in background
    if (system("gnome-terminal -- fluidsynth -a alsa") != 0) {
        perror("ERROR starting FluidSynth\n");
        exit(1);
    }

    // Wait a moment to ensure fluidsynth is ready
    sleep(10);

    // Execute aconnect to connect MIDI ports
    if (system("gnome-terminal -- aconnect 129:0 128:0 & aconnect -l") != 0) {
        perror("ERROR connecting MIDI ports with aconnect\n");
        exit(1);
    }
}

int main() {
    // Set up Ctrl+C signal handler
    signal(SIGINT, handle_sigint);

    // Start fluidsynth and connect the MIDI ports
    start_synth_and_connect();

    int serial_fd = configure_serial_port(SERIAL_PORT);
    printf("Player ready!\n");

    snd_rawmidi_t* midi_out;
    if (snd_rawmidi_open(NULL, &midi_out, "virtual", SND_RAWMIDI_NONBLOCK) < 0) {
        fprintf(stderr, "ERROR opening ALSA MIDI port.\n");
        return 1;
    }

    uint8_t midi_message[MIDI_MSG_SIZE];
    uint8_t buffer[MIDI_MSG_SIZE];
    int buffer_pos = 0;
    int k = 0;
    while (!should_exit) {
        // Read available bytes from the serial port
        int bytes_read = read(serial_fd, buffer + buffer_pos, MIDI_MSG_SIZE - buffer_pos);
        if (bytes_read > 0) {
            buffer_pos += bytes_read;

            if (buffer[0] == CTRL_C) {
                printf("Well Done!\n");
                break;
            }

            // Check if there is a full MIDI message
            if (buffer_pos == MIDI_MSG_SIZE) {
                // Copy the buffer into the MIDI message and reset buffer_pos
                memcpy(midi_message, buffer, MIDI_MSG_SIZE);
                buffer_pos = 0;

                printf("Read done!\n");
                printf("[it. %d]    ", k++);
                printf("MIDI message received: %02X %02X %02X\n", midi_message[0], midi_message[1], midi_message[2]);
                send_midi_message(midi_out, midi_message);
            }
        }
    }

    close(serial_fd);
    snd_rawmidi_close(midi_out);
    printf("Closed.\n");

    return 0;
}
