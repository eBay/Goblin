package com.ebay.goblin.client.model.common;

import com.google.protobuf.ByteString;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;


@Getter
@Setter
@ToString(callSuper = true)
public class GenerateResult extends AbstractResult {

    protected ByteString outputInfo;

}
