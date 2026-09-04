//Created by Salty on 8/25/26.

#import "PPMainViewController.h"

@implementation PPNSSwitch
@end

@implementation PPNSButton
@end

@interface PPMainViewController ()
@property (strong, nonatomic) PPTweaksCollection *tweaksCollection;
@property (strong) PPListEditorController *listEditorController;
@property (strong) PPTweakEditorController *tweakEditorController;
@end

@implementation PPMainViewController
- (void)awakeFromNib{
    [super awakeFromNib];

    self.tweaksTableView.selectionHighlightStyle = NSTableViewSelectionHighlightStyleNone;
    self.tweaksTableView.rowHeight = 50;
    self.tweaksTableView.columnAutoresizingStyle = NSTableViewLastColumnOnlyAutoresizingStyle;
    
    [self refreshDaemonBox:nil];
    
    [self reloadGlobalBlock];
    [self reloadTweaksList];
}

- (void)reloadGlobalBlock{
    NSDictionary *opts = [NSDictionary dictionaryWithContentsOfFile:(NSString *)kPPOptionsPath];

    if (!opts || opts.count < 1)
        return;

    if (opts[kPPOptionsDictDisablePAC])
        self.disablePAC = [opts[kPPOptionsDictDisablePAC] boolValue] ?: NO;

    if (opts[kPPOptionsDictPauseInjection])
        self.pauseInjection = [opts[kPPOptionsDictPauseInjection] boolValue] ?: NO;

    if (opts[kPPOptionsDictUseLegacyAmmonia])
        self.useLegacyAmmonia = [opts[kPPOptionsDictUseLegacyAmmonia] boolValue] ?: NO;

    if (opts[kPPOptionsDictGlobalBlacklist])
        self.globalBlacklist = [opts[kPPOptionsDictGlobalBlacklist] mutableCopy] ?: [NSMutableArray array];
}

- (void)reloadTweaksList{
    if (!self.tweaksCollection)
        self.tweaksCollection = [PPTweaksCollection loadCurrentTweaksContentsUsingAmmonia:self.useLegacyAmmonia];
    else
        [self.tweaksCollection setIsForLegacyAmmonia:self.useLegacyAmmonia];

    [self.tweaksTableView reloadData];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView{
    return self.tweaksCollection.collectionDictionary.count ?: 0;
}

- (NSView *)tableView:(NSTableView *)tableView
    viewForTableColumn:(NSTableColumn *)tableColumn
                   row:(NSInteger)row{

    NSUInteger columnIdx = tableColumn.identifier.integerValue;
    CGRect frame = CGRectMake(0, 0, tableColumn.width, tableView.rowHeight);

    if (columnIdx == 0){
        NSView *v = [[NSView alloc] initWithFrame:frame];
        NSImageView *iv = [[NSImageView alloc] initWithFrame:CGRectInset(frame, 5, 5)];
        iv.image = [NSImage imageNamed:@"Drill"];
        iv.editable = NO;
        iv.imageScaling = NSImageScaleProportionallyUpOrDown;
        [v addSubview:iv];
        return v;
    }

    if (columnIdx == 1){
        NSView *container = [[NSView alloc] initWithFrame:frame];

        //Name
        NSString *path = self.tweaksCollection.collectionDictionary.allKeys[row];
        NSString *name = [[[path lastPathComponent]
                           stringByReplacingOccurrencesOfString:@".options"
                                                     withString:@""]
                           stringByReplacingOccurrencesOfString:@".disabled"
                                                     withString:@""];

        NSFont *font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSControlSizeRegular]];
        NSDictionary *attributes = @{ NSFontAttributeName: font };

        CGSize constraintSize = CGSizeMake(CGFLOAT_MAX, frame.size.height);
        CGRect boundingBox = [name boundingRectWithSize:constraintSize
                                                options:NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingUsesFontLeading
                                             attributes:attributes
                                                context:nil];

        CGFloat textPadding = 8.0;
        CGFloat requiredWidth = ceil(boundingBox.size.width) + textPadding;

        NSTextField *tf = [[NSTextField alloc] initWithFrame:NSMakeRect(
                                                                        0,
                                                                        CGRectGetMidY(frame) - 11,
                                                                        MIN(requiredWidth,
                                                                            frame.size.width * .65),
                                                                        22
                                                                        )];
        
        tf.editable = NO;
        tf.bordered = NO;
        tf.drawsBackground = NO;
        tf.usesSingleLineMode = YES;
        tf.stringValue = name;
        tf.alignment = NSTextAlignmentLeft;

        [container addSubview:tf];
        
        // Edit Button
       
        NSStackView *editButtonSv = [[NSStackView alloc] initWithFrame:CGRectMake(frame.size.width * .60 + 60,
                                                                                     0,
                                                                                     80,
                                                                                     tableView.rowHeight)];
        
        [editButtonSv setSpacing:1];
        [editButtonSv setOrientation:NSUserInterfaceLayoutOrientationVertical];
        [container addSubview:editButtonSv];
        
        PPNSButton *editButton = [[PPNSButton alloc] initWithFrame:CGRectMake(0,
                                                                              0,
                                                                              30,
                                                                              tableView.rowHeight)];
        [editButton setBoundTweakMetafilePath:path];
        [editButton setImage:[NSImage imageWithSystemSymbolName:@"long.text.page.and.pencil" accessibilityDescription:nil]];
        [editButton setToolTip:@"Edit tweak configuration..."];
        [editButton setImagePosition:NSImageAbove];
        [editButton setTitle:@""];
        [editButton setTarget:self];
        [editButton setAction:@selector(editTweakOptions:)];
        [editButtonSv addView:editButton inGravity:NSStackViewGravityCenter];
        
        NSTextField *editButtonLabel = [[NSTextField alloc] initWithFrame:CGRectMake(0, 0, 80, 10)];
        editButtonLabel.editable = NO;
        editButtonLabel.drawsBackground = NO;
        editButtonLabel.bordered = NO;
        editButtonLabel.stringValue = @"Edit";
        [editButtonSv addView:editButtonLabel inGravity:NSStackViewGravityCenter];
        
        // Disable Button
        BOOL isEnabled = [[self.tweaksCollection.collectionDictionary objectForKey:path] boolValue];
        
        NSStackView *disableButtonSv = [[NSStackView alloc] initWithFrame:CGRectMake(frame.size.width * .60,
                                                                                     0,
                                                                                     80,
                                                                                     tableView.rowHeight)];
        [disableButtonSv setOrientation:NSUserInterfaceLayoutOrientationVertical];
        [disableButtonSv setAutoresizesSubviews:YES];
        [disableButtonSv setSpacing:1];
        
        PPNSButton *disableButton = [[PPNSButton alloc] initWithFrame:CGRectMake(frame.size.width * .65,
                                                                                 CGRectGetMidY(frame) - 10,
                                                                                 80,
                                                                                 10)];
        
        [disableButton setBoundTweakMetafilePath:path];
        [disableButton setTarget:self];
        disableButton.action = isEnabled ? @selector(disableTweak:) : @selector(enableTweak:);
        disableButton.state = isEnabled ? NSControlStateValueOn : NSControlStateValueOff;
        disableButton.title = @"Enabled";
        [disableButton setToolTip:@"Enable/Disable tweak..."];
        [disableButtonSv addView:disableButton inGravity:NSStackViewGravityCenter];
        [disableButton setButtonType:NSButtonTypeSwitch];
        
        [container addSubview:disableButtonSv];
        
        // Package Button
        NSStackView *packageButtonSv = [[NSStackView alloc] initWithFrame:CGRectMake(frame.size.width * .60 + 102,
                                                                                     0,
                                                                                     80,
                                                                                     tableView.rowHeight)];
        
        [packageButtonSv setOrientation:NSUserInterfaceLayoutOrientationVertical];
        [packageButtonSv setSpacing:1];
        [container addSubview:packageButtonSv];
        
        PPNSButton *packageButton = [[PPNSButton alloc] initWithFrame:CGRectMake(frame.size.width * .60 + 80,
                                                                              0,
                                                                              30,
                                                                              tableView.rowHeight)];
        [packageButton setBoundTweakMetafilePath:path];
        [packageButton setImage:[NSImage imageWithSystemSymbolName:@"shippingbox" accessibilityDescription:nil]];
        [packageButton setToolTip:@"Package tweak..."];
        [packageButton setImagePosition:NSImageAbove];
        [packageButton setTitle:@""];
        [packageButton setTarget:self];
        [packageButton setAction:@selector(packageTweak:)];
        [packageButtonSv addView:packageButton inGravity:NSStackViewGravityCenter];
        
        NSTextField *packageButtonLabel = [[NSTextField alloc] initWithFrame:CGRectMake(0, 0, 80, 10)];
        packageButtonLabel.editable = NO;
        packageButtonLabel.drawsBackground = NO;
        packageButtonLabel.bordered = NO;
        packageButtonLabel.stringValue = @"Pack";
        [packageButtonSv addView:packageButtonLabel inGravity:NSStackViewGravityCenter];
        
        return container;
    }

    return nil;
}

// Actions
- (IBAction)saveChanges:(NSButton *)sender{
    NSMutableDictionary *currentOptions = [NSMutableDictionary dictionary];
    [currentOptions setObject:@(self.disablePAC) forKey:kPPOptionsDictDisablePAC];
    [currentOptions setObject:self.globalBlacklist forKey:kPPOptionsDictGlobalBlacklist];
    [currentOptions setObject:@(self.pauseInjection) forKey:kPPOptionsDictPauseInjection];
    [currentOptions setObject:@(self.useLegacyAmmonia) forKey:kPPOptionsDictUseLegacyAmmonia];
    
    [currentOptions writeToFile:kPPOptionsPath
                     atomically:YES];
    
    [self reloadGlobalBlock];
    [self reloadTweaksList];
}

- (void)disableTweak:(PPNSSwitch *)sender{
    [self.tweaksCollection disableTweakAtPath:[sender boundTweakMetafilePath]];
    [self reloadTweaksList];
}

- (void)enableTweak:(PPNSSwitch *)sender{
    [self.tweaksCollection enableTweakAtPath:[sender boundTweakMetafilePath]];
    [self reloadTweaksList];
}

- (IBAction)openTweakDirectory:(NSButton *)sender{
    NSURL *URL = [NSURL fileURLWithPath:self.useLegacyAmmonia ? kPPTweaksDirectoryAmmonia : kPPTweaksDirectoryPluginPlayground
                            isDirectory:YES];
    
    [[NSWorkspace sharedWorkspace] openURL:URL];
}

- (void)packageTweak:(PPNSButton *)sender{
    [self.tweaksCollection packageTweakAtPath:[sender boundTweakMetafilePath]];
}

- (IBAction)globalBlacklistEditor:(NSButton *)sender{
    if (!self.listEditorController)
        self.listEditorController = [[PPListEditorController alloc] initWithDelegate:self];
    
    [self.listEditorController presentWithInitialArray:self.globalBlacklist ?: @[]];
}

- (void)updateGlobalBlacklistWithArray:(NSArray<NSString *> *)array{
    NSMutableDictionary *currentOptions = [NSMutableDictionary dictionaryWithContentsOfFile:kPPOptionsPath];
    [currentOptions setObject:[array mutableCopy] forKey:kPPOptionsDictGlobalBlacklist];
    
    [currentOptions writeToFile:kPPOptionsPath
                     atomically:YES];
    
    [self.globalBlacklist removeAllObjects];
    [self.globalBlacklist addObjectsFromArray:array];
}

- (IBAction)handleInstallButtonAction:(NSButton *)sender{
    if ([PPDaemonStatus plistExists]){
        [PPDaemonStatus removePlist];
        sender.title = @"Install";
    } else {
        [PPDaemonStatus addPlist];
        sender.title = @"Uninstall";
    }
}

- (IBAction)refreshDaemonBox:(NSButton *)sender{
    BOOL isFangsRunning = [PPDaemonStatus fangsIsRunning];
    self.tweakStatusImageView.image = isFangsRunning ? [NSImage imageNamed:@"NSStatusAvailable"] : [NSImage imageNamed:@"NSStatusUnavailable"];
    self.tweakStatusLabel.stringValue = isFangsRunning ? @"Fangs is running." : @"Fangs is not running.";
    self.installControlButton.title = [PPDaemonStatus plistExists] ? @"Uninstall" : @"Install";
}

- (IBAction)editTweakOptions:(PPNSButton *)sender{
    NSString *optionPath = [sender boundTweakMetafilePath];
    
    NSString *name = [[[optionPath lastPathComponent]
                       stringByReplacingOccurrencesOfString:@".options"
                                                 withString:@""]
                       stringByReplacingOccurrencesOfString:@".disabled"
                                                 withString:@""];
    
    if (self.tweakEditorController)
        [[[self.tweakEditorController view] window] close];
    
     self.tweakEditorController = [[PPTweakEditorController alloc] initWithTitle:name
                                                                 optionsDictPath:optionPath
                                                                tweakOptionsDict:[NSDictionary dictionaryWithContentsOfFile:optionPath]];
    
    [[[self.tweakEditorController view] window] makeKeyAndOrderFront:nil];
}
@end
