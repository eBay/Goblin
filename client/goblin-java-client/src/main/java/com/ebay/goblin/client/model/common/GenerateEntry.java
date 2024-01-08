package com.ebay.goblin.client.model.common;

import com.google.protobuf.ByteString;
import goblin.proto.Userdefine;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

import java.util.List;


@Getter
@Setter
@ToString(callSuper = true)
public class GenerateEntry extends AbstractEntry {

    protected Userdefine.UserDefine.CommandType cmdType;
    protected List<KeyType> srcKeys;
    protected List<KeyType> tgtKeys;
    protected ByteString inputInfo;
}
