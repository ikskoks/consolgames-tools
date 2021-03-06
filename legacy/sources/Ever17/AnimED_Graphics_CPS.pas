{
  CPS decoding/encoding module
  Copyright � 2007 HoRRoR & Serke. Open Source.
  http://consolgames.jino-net.ru
}
unit AnimED_Graphics_CPS;

interface

uses Windows, SysUtils, Classes;

Type
  TParam = Array[0..2] of Byte;
  TProgrProc = procedure(I: Integer);

const coDecompress     = 0;
      coCompressOnly   = 1;
      coCompressEncode = 2;

procedure CPS_Stream(var Stream : TStream; Operation : byte);

function CPS_Decompress(var Buf : pointer; Size: Integer): Integer;
function CPS_Compress(var Buf : pointer; Size: Integer; fBLK,fLZ,fRLE: Boolean; Progress: TProgrProc = NIL): Integer;
{procedure ShowMem(P: Pointer; Size: Integer);}
implementation

procedure CPS_Stream(var Stream : TStream; Operation : byte);
var Buf: Pointer;
begin
 Getmem(Buf, Stream.Size);
 Stream.Read(Buf^, Stream.Size);
 Stream.Position := 0;
 case Operation of
  coDecompress       : Stream.Size := CPS_Decompress(Buf, Stream.Size);
  coCompressOnly     : Stream.Size := CPS_Compress(Buf, Stream.Size, True,True,True);
  coCompressEncode   : ;
 end;
 Stream.Write(Buf^, Stream.Size);
 FreeMem(Buf);
end;

function CPS_Decompress(var Buf : pointer; Size: Integer): Integer;
var WBuf: Pointer;
    B,WB,RB: ^Byte;
    Sz,v1_32,lCount32,lCount32_2,v2_32: DWord;
    C: Byte;
begin
 GetMem(WBuf,Size+2048);
 WB:=Addr(WBuf^);
 B:=Addr(Buf^);
 Sz:=DWord(WB);
 While Sz+Size>DWord(WB) do
  begin
   //WriteLn(Format('%d/%d',[Integer(WB),Sz+Size]));
   C:=B^;
   Inc(B);
   Case (C SHR 6) of
    3:                              //RLE
      begin
       v1_32 := ( c and $1F ) + 2;
       if ( c and $20 ) <> 0 then
        begin
         v1_32 := v1_32 + ( B^ SHL 5 );
         Inc( B );
        end;
       c := B^; { ������, ������� ����� ������� � ���. ����� }
       Inc(B);
       lCount32 := 0;
       While lCount32 < v1_32 do
       begin
        WB^ := c;
        Inc( WB );
        Inc( lCount32 );
       end;
      end;
    2:                              //LZ
      begin
       RB:=WB;
       Inc(RB,- ((c and 3) SHL 8) - B^ - 1);
       v1_32 := ((c SHR 2) and $0F) + 2; { ���������� ����� ����� }
       Inc( B );
       lCount32 := 0;
       while lCount32 < v1_32 do
        begin
         WB^ := RB^;
         Inc(WB);
         Inc(RB);
         Inc(lCount32);
        end;
      end;
    1:                              //BLK
      begin
       lCount32_2 := 0;
       v1_32 := ( c and $3F ) + 2;
       v2_32 := B^ + 1;
       Inc( B );
       while lCount32_2  < v2_32 do
        begin
         lCount32 := 0;
         while lCount32 < v1_32 do
          begin
           RB := B;   Inc(RB,lCount32);
           WB^:=RB^; Inc(WB); Inc(lCount32);
          end;
         Inc( lCount32_2 );
        end;
       Inc(B,v1_32);
      end;
    0:                              //CPY
      begin
       v1_32 := ( c and $1F ) + 1;
       if c and $20 <> 0 then
       begin
        v1_32 := v1_32 + ((B^) SHL 5);
        Inc( B );
       end;
       lCount32 := 0;
       while lCount32 < v1_32 do
        begin
         WB^ := B^; Inc( WB ); Inc( B ); Inc( lCount32 );
        end;
      end;
    end;
  end;
  FreeMem(Buf);
  Buf:=WBuf;
  Result:=Size;
end;

{procedure CBlockCPY(var P,WP: Pointer; Count: Integer);
var B,WB: ^Byte; n: Integer;
begin
 B:=P; WB:=WP;
 Inc(B,-Count);
 WB^:=(Byte(Count>$20) SHL 5)+((Count-1) AND $1F);
 Inc(WB);
 If Count>$20 Then
  begin
   WB^:=$FF and ((Count-1) SHR 5);
   Inc(WB);
  end;
 For n:=0 To Count-1 do
  begin
   WB^:=B^;
   Inc(B);
   Inc(WB);
  end;
 WP:=WB; P:=B;
end;}

function CBlockLZ(var P,WP: Pointer; MaxLink,MaxSize: Integer; Test: Boolean): Integer;
var  nRB,nB,B,WB: ^Byte;
Best,bLink,n,m,Count: Integer;
begin
 Result:=-15;
 If MaxLink<2 Then Exit;
 If MaxSize>16 Then MaxSize:=16;
 B:=P; WB:=WP; Best:=-1;
 If MaxLink>1024 Then MaxLink:=1024;
 For m:=MaxLink downto 1 do
 begin
  Count:=0; nRB:=B; Inc(nRB,-m);  nB:=B;
  For n:=0 To MaxSize do
   begin
    If nRB=B Then Inc(nRB,-m);
    If nB^<>nRB^ Then Break;
    Inc(nB); Inc(nRB); Inc(Count);
   end;
  If Count>Best Then begin Best:=Count; bLink:=m; End;
  If Count=16 Then break;
 end;
 WB^:=$80+(((Best-2) SHL 2) AND $3C)+(((bLink-1) SHR 8) AND $3);
 Inc(WB);
 WB^:=(bLink-1) AND $FF;
 Inc(WB);
 //If not Test Then Inc(B,Best) else Inc(WB,-2);
 Inc(B,Best);
 If not Test Then begin P:=B; WP:=WB; end;
 Result:=Best-2;
end;

function CBlockRLE(var P,WP: Pointer; MaxSize: Integer; Test: Boolean): Integer;
var B,WB: ^Byte; C: Byte; BSize,Size: Integer;
begin
 B:=P; WB:=WP; C:=B^; Inc(B); Result:=1; Size:=0;
 while B^=C do
  begin
   Inc(Result); Inc(B);
   If (Result>=$2001) or (Result>MaxSize) Then break;
  end;
 BSize:=Result;
 WB^:=$C0+(Byte(Result>$21) SHL 5)+((Result-2) AND $1F);
 Inc(WB);
 If (Result>$21) Then
  begin
   WB^:=$FF and ((Result-2) SHR 5);
   Inc(Size); Inc(WB);
  end;
 WB^:=C; Inc(WB); Inc(Size,2);
 if Result>$21 then Result:=Result-3 Else Result:=Result-2;
 //If Test Then Begin Inc(WB,-Size); Inc(B,-BSize); end;
 if not Test then Begin P:=B; WP:=WB; end;
end;

function CBlockBLK(var P,WP: Pointer; MaxSize: Integer; Test: Boolean): Integer;
var B,WB,RB,RB2: ^Byte; n,m,l,Best,BestLen,BestCnt,Max,MaxC: Integer;
Label brk;
begin
 Result:=-15; Best:=-15;
 If MaxSize<6 Then Exit;
 B:=P; WB:=WP;
 Max:=MaxSize div 2;
 If Max>$3F+2 Then Max:=$41;
 For m:=2 To Max do
 begin
  RB2:=B; Inc(RB2,m);
  MaxC:=MaxSize div m;
  If MaxC>$FF Then MaxC:=$FF;
  for n:=1 To MaxC do
   begin
    RB:=B;
    For l:=1 To m do
     begin
      If RB^<>RB2^ Then GoTo brk;
      Inc(RB);
      Inc(RB2);
     end;
   end;
brk:
  If m*(n-1)-2>=Best Then
   begin
    Best:=m*(n-1)-2;
    BestCnt:=n; BestLen:=m;
   end;
  end;
  Result:=Best;
  If Best<0 Then Exit;
  WB^:=$40+((BestLen-2) AND $3F); Inc(WB);
  WB^:=(BestCnt-1) AND $FF; Inc(WB);
  If not Test Then
  begin
    For n:=0 To BestLen-1 do
    begin
     WB^:=B^;
     Inc(WB);
     Inc(B);
    end;
    Inc(B,BestLen*(BestCnt-1));
    P:=B; WP:=WB;
  end;
end;

function CPS_Compress(var Buf: pointer; Size: Integer; fBLK,fLZ,fRLE: Boolean; Progress: TProgrProc = NIL): Integer;
var WBuf : Pointer;
    B,WB : ^Byte;
    Sz,Num,WSz,M,Count,Vr,n,ReadCount : Integer;
    tFlag: Boolean;
begin
 GetMem(WBuf,Size+(Size div $800)+4);
 B:=Addr(Buf^);
 WB:=Addr(WBuf^);
 Sz:=Integer(B);
 WSz:=Integer(WB);
 ReadCount:=0;
 while (Sz+Size>Integer(B)) or (ReadCount>0) do
  begin
    Count:=-15;
    Vr:=0;
    For n:=1 To 4 do
    begin
      tFlag:=(n<4);
      If not tFlag then
      begin
        Num:=M;
        If (ReadCount<$2000) and (Count<1) and (Sz+Size>Integer(B)) Then
        begin
          Inc(ReadCount); Inc(B); Num:=0;// Inc(WB);
        end else
        If ReadCount>0 Then
        begin
          CBlockCPY(Pointer(B),Pointer(WB),ReadCount);
          ReadCount:=0;
          If (Sz+Size<=Integer(B)) Then Num:=0;
        end;
      end else Num:=n;
      Case Num of    // ����� ������������� ������ ������ �������
        1: If fBLK Then Vr:=CBlockBLK(Pointer(B),Pointer(WB),(Sz+Size)-Integer(B), tFlag);
        2: If fLZ  Then Vr:=CBlockLZ( Pointer(B),Pointer(WB),(Integer(B)-Sz),(Sz+Size)-Integer(B), tFlag);
        3: If fRLE Then Vr:=CBlockRLE(Pointer(B),Pointer(WB),(Sz+Size)-Integer(B), tFlag);
      end;
      If Count<Vr Then begin Count:=Vr; M:=n; End;
    end;
  If @Progress<>NIL Then Progress(Integer(B)-Sz); 
  end;
  FreeMem(Buf);
  Buf:=WBuf;
  Result:=Integer(WB)-WSz;
end;

{procedure ShowMem(P: Pointer; Size: Integer);
var B: ^Byte; n,C: Integer; S,S2: String;
begin
  B:=P; C:=0;
  While C<=Size do
  begin
    S:=''; S2:='';
    For n:=0 To 15 do
    begin
      If B^>$1F Then S2:=S2+Char(B^) Else S2:=S2+'.';
      S:=Format('%s%s ',[S,IntToHex(B^,2)]);
      Inc(B); Inc(C);
      If C>Size Then Exit;
    end;
    WriteLn(Format('%s: %s  %s',[IntToHex(Integer(B)-16,8),S,S2]));
  end;
end;}

end.
 