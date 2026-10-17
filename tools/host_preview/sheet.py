import sys, glob, os, zlib, struct
def read_ppm(p):
    d=open(p,'rb').read(); parts=d.split(b'\n',3); w,h=map(int,parts[1].split()); return w,h,parts[3]
def write_png(path,w,h,rgb):
    raw=b''.join(b'\x00'+rgb[y*w*3:(y+1)*w*3] for y in range(h))
    def chunk(t,dat): c=struct.pack('>I',len(dat))+t+dat; return c+struct.pack('>I',zlib.crc32(t+dat)&0xffffffff)
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,6))+chunk(b'IEND',b''))
out=sys.argv[1]; names=sys.argv[2:]
S=2; gap=6
imgs=[read_ppm(os.path.join(out,n+'.ppm')) for n in names]
cols=2; rows=(len(imgs)+cols-1)//cols
W=cols*(320*S+gap)+gap; H=rows*(240*S+gap)+gap
canvas=bytearray(b'\x30\x30\x80'*(W*H))
for i,(w,h,d) in enumerate(imgs):
    ox=gap+(i%cols)*(320*S+gap); oy=gap+(i//cols)*(240*S+gap)
    for y in range(h*S):
        row=bytearray()
        src=d[(y//S)*w*3:(y//S+1)*w*3]
        for x in range(w): row+=src[x*3:x*3+3]*S
        o=((oy+y)*W+ox)*3
        canvas[o:o+len(row)]=row
write_png(os.path.join(out,'sheet_'+names[0]+'.png'),W,H,bytes(canvas))
print(names)
