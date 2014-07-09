
#include <iostream>
#include <iomanip>
#include <fstream>
#include <deque>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
#include <sstream>

using namespace std;

#define OP_HALT 0
#define OP_SET 1
#define OP_PUSH 2
#define OP_POP 3
#define OP_EQ 4
#define OP_GT 5
#define OP_JMP 6
#define OP_JT 7
#define OP_JF 8
#define OP_ADD 9
#define OP_MULT 10
#define OP_MOD 11
#define OP_AND 12
#define OP_OR 13
#define OP_NOT 14
#define OP_RMEM 15
#define OP_WMEM 16
#define OP_CALL 17
#define OP_RET 18
#define OP_OUT 19
#define OP_IN 20
#define OP_NOOP 21

static inline unsigned short __builtin_bswap16(unsigned short a)
{
	return (a<<8)|(a>>8);
}

bool is_reg(unsigned short v)
{
	return v >= 32768 && v < 32776;
}

int reg_index(unsigned short v)
{
	return v - 32768;
}

string val_string(unsigned short v)
{
	stringstream ss;
	if(is_reg(v)) ss << "R" << reg_index(v);
	else ss << v;
	return ss.str();
}

unsigned short lor(unsigned short v, unsigned short * regs)
{
	if(v < 32768) return v;
	if(v < 32776) return regs[v-32768];
	cout << "\nWARNING: Invalid number encountered\n";
	return 0;
}

void setreg(unsigned short reg, unsigned short val, unsigned short * regs)
{
	val = lor(val, regs);
	if(reg < 32768)
	{
		cout << "\nWARNING: Invalid register encountered\n";
		return;
	}
	if(reg < 32776) regs[reg-32768] = val;
	else
		cout << "\nWARNING: Invalid register encountered\n";
	return;
}


streampos size;
unsigned char * memblock;
unsigned short registers[8];
deque <unsigned short> vm_stack;
unsigned int prog_counter = 0;
unsigned short * args;
unsigned short opcode;

deque<unsigned char> commands;

bool halt_vm = false;
bool debug = false;
bool trace = false;
ofstream trace_file;

int break_on_register = -1;

int num_op_args(unsigned short code)
{
	switch(code)
	{
		case OP_HALT:
			return 0;
		case OP_SET:
			return 2;
		case OP_PUSH:
			return 1;
		case OP_POP:
			return 1;
		case OP_EQ:
			return 3;
		case OP_GT:
			return 3;
		case OP_JMP:
			return 1;
		case OP_JT:
			return 2;
		case OP_JF:
			return 2;
		case OP_ADD:
			return 3;
		case OP_MULT:
			return 3;
		case OP_MOD:
			return 3;
		case OP_AND:
			return 3;
		case OP_OR:
			return 3;
		case OP_NOT:
			return 2;
		case OP_RMEM:
			return 2;
		case OP_WMEM:
			return 2;
		case OP_CALL:
			return 1;
		case OP_RET:
			return 0;
		case OP_OUT:
			return 1;
		case OP_IN:
			return 1;
		case OP_NOOP:
			return 0;
		default:
			return 0;
	}
}

string opCodeStr(unsigned short code)
{
	switch(code)
	{
		case OP_HALT:
			return "HALT";
		case OP_SET:
			return "SET";
		case OP_PUSH:
			return "PUSH";
		case OP_POP:
			return "POP";
		case OP_EQ:
			return"EQ";
		case OP_GT:
			return"GT";
		case OP_JMP:
			return "JMP";
		case OP_JT:
			return"JT";
		case OP_JF:
			return"JF";
		case OP_ADD:
			return "ADD";
		case OP_MULT:
			return "MULT";
		case OP_MOD:
			return "MOD";
		case OP_AND:
			return "AND";
		case OP_OR:
			return"OR";
		case OP_NOT:
			return "NOT";
		case OP_RMEM:
			return "RMEM";
		case OP_WMEM:
			return "WMEM";
		case OP_CALL:
			return "CALL";
		case OP_RET:
			return "RET";
		case OP_OUT:
			return "OUT";
		case OP_IN:
			return"IN";
		case OP_NOOP:
			return "NOOP";
		default:
			return "UNKNOWN";
	}
}

void write_trace_file()
{
	if(!trace_file.is_open())
	{
		trace_file.open("trace.txt", ios::trunc | ios::out);
	}
	if(trace_file.is_open())
	{
		trace_file << left << setw(6) << prog_counter << setw(6) << prog_counter/2
		<< setw(5) << opCodeStr(opcode);
		for (int i = 0; i < num_op_args(opcode); ++i)
		{
			trace_file << setw(7) << val_string(args[i]);
		}
		trace_file << endl;
	}
}

void print_vm_state()
{
	cout <<
	"\nCURRENT INSTRUCTION: " << opCodeStr(opcode);
	for (int i = 0; i < num_op_args(opcode); ++i)
	{
		cout << ", " << val_string(args[i]);
	}
	cout <<
	"\nPROGRAM COUNTER:     " << prog_counter/2 <<
	"\nBYTE OFFSET:         " << prog_counter <<
	"\nREGISTERS:\n" <<
	"\nR0:                  " << val_string(registers[0]) <<
	"\nR1:                  " << val_string(registers[1]) <<
	"\nR2:                  " << val_string(registers[2]) <<
	"\nR3:                  " << val_string(registers[3]) <<
	"\nR4:                  " << val_string(registers[4]) <<
	"\nR5:                  " << val_string(registers[5]) <<
	"\nR6:                  " << val_string(registers[6]) <<
	"\nR7:                  " << val_string(registers[7]) <<
	"\nSTACK:               ";
	for (int i = 0; i < vm_stack.size() && i < 100; ++i)
	{
		cout << val_string(vm_stack[i]) << ", ";
	}
	cout << endl;
}

void write_mem_contents()
{
	ofstream out_file ("out.bin", ios::binary | ios::trunc | ios::out);
	out_file.write((char*)memblock, size);
	out_file.close();
}

void int_handler (int sig)
{
	cout << "\n\nCaught signal " << sig << endl;

	debug = !debug;

	cout << "Debug mode is now ";
	if(debug) cout << "on" << endl;
	else cout << "off" << endl;
}

void debugger()
{
	bool done = false;
	while(!done)
	{
		print_vm_state();
		write_mem_contents();

		cout << "Debug options:\n"
		<< "q:   quit program\n"
		<< "0-7: to alter register values\n"
		<< "o:   alter current opcode\n"
		<< "s:   push value to stack\n"
		<< "p:   pop value off stack\n"
		<< "m:   alter program memory address\n"
		<< "w:   watch for register being used\n"
		<< "t:   trace output to trace.txt\n"
		<< "d:   exit debug mode\n"
		<< "any other key to step forward.\n";
		char in;
		cin >> in;

		unsigned short value, value2;
		int intput;

		switch(in)
		{
			case 'q':
				cout << "Goodbye!\n";
				halt_vm = true;
				debug = false;
				done = true;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				cout << "enter new value for register " << in << endl;
				cin >> value;
				registers[(int)(in - '0')] = value;
				break;
			case 'o':
				cout << "enter new value for opcode\n";
				cin >> value;
				opcode = value;
				break;
			case 's':
				cout << "enter new value to push onto stack\n";
				cin >> value;
				vm_stack.push_front(value);
				break;
			case 'p':
				cout << "popped off the top of the stack.\n";
				vm_stack.pop_front();
				break;
			case 'm':
				cout << "enter memory address to change\n";
				cin >> value;
				if(value >= size)
				{
					cout << "invalid memory address.\n";
				}
				else
				{
					cout << "enter new value for address " << value << endl;
					cin >> value2;
					((unsigned short*)memblock)[value] = value2;
				}
				break;
			case 'w':
				cout << "enter number of register to watch for (0-7) -1 disables\n";
				cin >> intput;
				if(intput < -1 || intput > 7)
				{
					cout << "invalid register";
				}
				else
				{
					break_on_register = intput;
				}
				break;
			case 't':
				trace = !trace;
				cout << "tracing is now " << (trace ? "on\n" : "off\n") ;
				break;
			case 'd':
				debug = false;
				done = true;
				break;
			default:
				done = true;
				break;
		}
	}
	// string temp;
	// cin.getline(temp, STRLEN);
	// if (cin.fail()) {
	// 	cin.clear();
	// 	cin.ignore(numeric_limits<streamsize>::max(), '\n');
	// }
}

int main ()
{
	signal(SIGINT, int_handler);
	ifstream file ("challenge.bin", ios::in|ios::binary|ios::ate);
	if (file.is_open())
	{
		size = file.tellg();
		memblock = new unsigned char [size];
		file.seekg (0, ios::beg);
		file.read ((char*)memblock, size);
		file.close();

		cout << "The entire file content is in memory. Begin running program:\n\n";

		file.open("commands.txt", ios::in);
		if (file.is_open())
		{
			while(!file.eof())
			{
				commands.push_back(file.get());
			}
			commands.pop_back();
		}

		while(prog_counter < size && !halt_vm)
		{
			opcode = ((unsigned short*)memblock)[prog_counter/2];
			args = ((unsigned short*)memblock)+(prog_counter+2)/2;

			unsigned char input;
			unsigned short temp[16];

			// see if any of the arguments used by this operation are a register we are watching for
			for (int i = 0; i < num_op_args(opcode); ++i)
			{
				if (is_reg(args[i]) && (int)(reg_index(args[i])) == break_on_register)
				{
					debug = true;
					cout << "\nbreak on register " << break_on_register << " triggered debug mode\n";
					break;
				}
			}

			if (debug)
			{
				debugger();
			}

			if(trace)
			{
				write_trace_file();
			}

			prog_counter +=2;
			switch(opcode)
			{
				case OP_HALT: //halt: 0
					// stop execution and terminate the program
					prog_counter = size;
					halt_vm = true;
					cout << "\nVM executed halt command\n";
					break;
				case OP_SET: // set: 1 a b
					// set register <a> to the value of <b>
					setreg(args[0], args[1], registers);
					prog_counter += 4;
					break;
				case OP_PUSH: // push: 2 a
					// push <a> onto the stack
					vm_stack.push_front(lor(args[0], registers));
					prog_counter += 2;
					break;
				case OP_POP: // pop: 3 a
					// remove the top element from the stack and write it into <a>; empty stack = error
					if (vm_stack.empty())
					{
						cout << "\nERROR: stack is empty\n";
					}
					else
					{
						setreg(args[0], vm_stack.front(), registers);
						vm_stack.pop_front();
					}

					prog_counter += 2;
					break;
				case OP_EQ: // eq: 4 a b c
					// set <a> to 1 if <b> is equal to <c>; set it to 0 otherwise
					setreg(args[0], (lor(args[1],registers) == lor(args[2],registers) ? 1 : 0), registers);
					prog_counter += 6;
					break;
				case OP_GT: // gt: 5 a b c
					setreg(args[0], (lor(args[1],registers) > lor(args[2],registers) ? 1 : 0), registers);
					prog_counter += 6;
					break;
				case OP_JMP: // jmp: 6 a
					// jump to <a>
					prog_counter = lor( args[0], registers ) * 2;
					break;
				case OP_JT: // jt: 7 a b
					// if <a> is nonzero, jump to <b>
					if(lor( args[0], registers ) != 0) {
						prog_counter = lor( args[1], registers ) * 2;
					}
					else {
						prog_counter += 4;
					}
					break;
				case OP_JF: // jf: 8 a b
					// if <a> is zero, jump to <b>
					if(lor( args[0], registers ) == 0)
						prog_counter = lor( args[1], registers ) * 2;
					else
						prog_counter +=4;
					break;
				case OP_ADD: // add: 9 a b c
					// assign into <a> the sum of <b> and <c> (modulo 32768)
					setreg(args[0], (lor(args[1],registers) + lor(args[2],registers))%32768, registers);
					prog_counter += 6;
					break;
				 case OP_MULT: // mult: 10 a b c
					// store into <a> the product of <b> and <c> (modulo 32768)
					setreg(args[0], (lor(args[1],registers) * lor(args[2],registers))%32768, registers);
					prog_counter += 6;
					break;
				case OP_MOD: // mod: 11 a b c
					// store into <a> the remainder of <b> divided by <c>
					setreg(args[0], (lor(args[1],registers) % lor(args[2],registers)), registers);
					prog_counter += 6;
					break;
				case OP_AND: // and: 12 a b c
					//stores into <a> the bitwise and of <b> and <c>
					setreg(args[0], (lor(args[1],registers) & lor(args[2],registers)), registers);
					prog_counter += 6;
					break;
				case OP_OR: // or: 13 a b c
					// stores into <a> the bitwise or of <b> and <c>
					setreg(args[0], (lor(args[1],registers) | lor(args[2],registers)), registers);
					prog_counter += 6;
					break;
				case OP_NOT: // not: 14 a b
					// stores 15-bit bitwise inverse of <b> in <a>
					temp[0] = lor(args[1], registers);
					temp[1] = ~temp[0];
					temp[2] = 32767;
					temp[0] = temp[1] & temp[2];
					setreg(args[0], temp[0], registers);
					prog_counter += 4;
				 	break;
				case OP_RMEM: // rmem: 15 a b
					// read memory at address <b> and write it to <a>
					setreg(args[0], ((unsigned short*)memblock)[lor(args[1],registers)], registers );
					prog_counter += 4;
					break;
				case OP_WMEM: // wmem: 16 a b
					// write the value from <b> into memory at address <a>
					((unsigned short*)memblock)[lor(args[0],registers)] = lor(args[1],registers);
					prog_counter += 4;
					break;
				case OP_CALL: // call: 17 a
					// write the address of the next instruction to the stack and jump to <a>
					vm_stack.push_front((prog_counter + 2)/2);
					prog_counter = lor( args[0], registers ) * 2;
					break;
				case OP_RET: // ret: 18
					// remove the top element from the stack and jump to it; empty stack = halt
					if (vm_stack.empty())
					{
						cout << "\nStack empty, halting\n";
					}
					else
					{
						prog_counter = vm_stack.front() * 2;
						vm_stack.pop_front();
					}
					break;
				case OP_OUT: // write the character represented by ascii code <a> to the terminal
					cout << (char)(lor( args[0], registers ));
					prog_counter += 2;
					break;
				case OP_IN: // read a character from the terminal and write its ascii code to <a>;
					// it can be assumed that once input starts, it will continue until a newline is encountered;
					// this means that you can safely read whole lines from the keyboard and trust that they will be fully read
					if(!commands.empty())
					{
						input = commands.front();
						commands.pop_front();
						cout << input;
					}
					else
					{
						input = cin.get();
					}
					setreg(args[0], (unsigned short)input, registers);
					prog_counter += 2;
					break;
				case OP_NOOP:
				default:
					break;
			}
		}
		cout << "\n\nDone\n";

		if(trace_file.is_open())
		{
			trace_file.close();
		}

		delete[] memblock;
	}
	else cout << "Unable to open file\n";
	return 0;
}