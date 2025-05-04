# bin2header.py
#
#使用例
#python bin2header.py MyFont.vlw fonts/MyMainFont.h my_main_font_vlw

import sys

def convert_bin_to_header(bin_file, header_file, var_name):
    with open(bin_file, 'rb') as f:
        data = f.read()
    
    with open(header_file, 'w', encoding='utf-8') as f:
        f.write('#ifndef _%s_H_\n' % var_name.upper())
        f.write('#define _%s_H_\n\n' % var_name.upper())
        f.write('#include <stdint.h>\n\n')
        f.write('// 自動生成されたフォントデータ\n')
        f.write('// 元ファイル: %s\n' % bin_file)
        f.write('// データサイズ: %d バイト\n\n' % len(data))
        
        f.write('static const uint8_t %s[] = {\n' % var_name)
        
        # バイナリデータを16進数形式で出力
        for i in range(0, len(data), 16):
            line = data[i:i+16]
            hex_values = ['0x%02x' % b for b in line]
            f.write('    %s,\n' % ', '.join(hex_values))
        
        f.write('};\n\n')
        f.write('#endif // _%s_H_\n' % var_name.upper())

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print('Usage: python bin2header.py <bin_file> <header_file> <variable_name>')
        sys.exit(1)
    
    bin_file = sys.argv[1]
    header_file = sys.argv[2]
    var_name = sys.argv[3]
    
    convert_bin_to_header(bin_file, header_file, var_name)
    print(f'Converted {bin_file} to {header_file} with variable name {var_name}')