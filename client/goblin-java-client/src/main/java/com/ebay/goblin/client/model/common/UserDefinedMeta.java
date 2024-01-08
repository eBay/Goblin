package com.ebay.goblin.client.model.common;

import lombok.Builder;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

import java.util.List;

@Getter
@Setter
@ToString(callSuper = true)
@Builder
public class UserDefinedMeta {
    List<Long> uintField;
    List<String> strField;

    public goblin.proto.Userdefine.UserDefinedMeta toProto() {
        goblin.proto.Userdefine.UserDefinedMeta.Builder builder = goblin.proto.Userdefine.UserDefinedMeta.newBuilder();
        if(uintField != null) {
            uintField.forEach(builder::addUintField);
        }
        if(strField != null) {
            strField.forEach(builder::addStrField);
        }
        return builder.build();
    }
}
