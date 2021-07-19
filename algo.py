f = open("json.txt", 'r')
s = f.readline()

buff = []
lenBlock = 256
outWords = []

searchWords = ['Devices":[{', 'DeviceName":','Device":{','Power":']
searchIdx = 0
for i in range(0,len(s),lenBlock):
    k = s[i:i+lenBlock]
    
    if buff == []:
        buff = k
    else:
        k, buff = buff+k, k
        
    #print(i, k.find(searchWords[3]))
        
    if searchIdx > len(searchWords)-1:
        searchIdx = 0
        
    for j in range(searchIdx, len(searchWords)):
        idx = k.find(searchWords[j])
        if idx != -1:
            searchIdx += 1
            lenData = 0
            
            if len(k) - idx < 40:
                searchIdx -= 1
                break;
                
            for y in range(idx, len(k)):
                if k[y] != ',':
                    lenData += 1
                else:
                    break;
                    
            if j == 1 or j == 3:
                print(i,idx, k[idx+len(searchWords[j]):idx+lenData].replace('"',''))
            #print(k)
        else:
            break;
