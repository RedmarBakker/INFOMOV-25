Build instructions:
- Follow the instructions from README.md
- cd in the folder cache25
- bootstrap ```build.sh```
- run ```sh build.sh``` in the terminal
- Run configuration can be changed in:
    template.cpp --> std::vector<Config> configurations (line 337)
    Select one or more configuration to run

Important note!
When configuring the experiments in template.cpp, on line 530;
    set the limit of framenr to 100 if ACCESS PATTERN == BUDDHA and 13000 else;